/**
 * @file network/NetworkClient.cpp
 * @brief TCP connection to the zappy_server — implementation.
 * @details Responsibility: implement the POSIX socket lifecycle (connect, send, recv,
 *          close), the WELCOME/GRAPHIC handshake, the bootstrap sequence, the line-
 *          buffered receive loop on a background thread, and the full protocol parser.
 *
 *          Architecture: this translation unit is the only place where <sys/socket.h>,
 *          <netdb.h>, and <unistd.h> are included. The rest of the application never
 *          sees raw POSIX types. All errors are converted into ZappyException subclasses
 *          before leaving this file.
 *
 */

#include "network/NetworkClient.hpp"
#include "exceptions.hpp"

#include <cerrno>
#include <cstring>
#include <sstream>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <spdlog/spdlog.h>

namespace {

/**
 * @brief Custom deleter for the addrinfo* returned by getaddrinfo().
 * @details getaddrinfo() allocates a linked list of addrinfo structs on the heap.
 *          The caller is responsible for calling freeaddrinfo() to release them.
 *          Wrapping the pointer in std::unique_ptr<addrinfo, AddrInfoDeleter>
 *          guarantees that freeaddrinfo() is called even if an exception is thrown
 *          before we finish using the result — no manual cleanup needed.
 */
struct AddrInfoDeleter {
    /**
     * @brief Calls freeaddrinfo() on the given pointer.
     * @param p addrinfo* returned by getaddrinfo(). May be nullptr (freeaddrinfo handles it).
     */
    void operator()(addrinfo* p) const noexcept
    {
        ::freeaddrinfo(p);
    }
};

/**
 * @brief Parse a player/egg ID token that the protocol prefixes with '#'.
 * @details The Zappy protocol uses tokens like "#5" to identify players and eggs.
 *          This helper strips the leading '#' and converts the remainder to uint32_t.
 * @param token The raw token string, e.g. "#5" or "#42".
 * @return Parsed numeric ID.
 * @throws NetworkParseException if the token does not start with '#' or is not numeric.
 */
static uint32_t readId(const std::string& token)
{
    if (token.empty() || token[0] != '#') {
        throw NetworkParseException("Expected '#'-prefixed ID, got: " + token);
    }
    try {
        return static_cast<uint32_t>(std::stoul(token.substr(1)));
    } catch (const std::exception&) {
        throw NetworkParseException("Non-numeric ID token: " + token);
    }
}

/**
 * @brief Parse a 7-element resource array from a stream.
 * @details Reads seven consecutive uint32_t values from @p ss into @p arr.
 *          Used for both tile content (bct) and player inventory (pin) lines.
 * @param ss Input stream positioned just before the first resource quantity.
 * @param arr Output array to fill.
 * @throws NetworkParseException if fewer than 7 values are available or any is non-numeric.
 */
static void readResources(std::istringstream& ss, std::array<uint32_t, 7>& arr)
{
    for (std::size_t i = 0; i < 7; ++i) {
        if (!(ss >> arr[i])) {
            throw NetworkParseException("Expected 7 resource values, failed at index "
                                        + std::to_string(i));
        }
    }
}

} // anonymous namespace

/**
 * @brief Construct with host and port. Does NOT open the socket.
 */
NetworkClient::NetworkClient(std::string host, uint16_t port)
    : _host(std::move(host)), _port(port)
{
}

/**
 * @brief Destructor: signal the recv thread, shut down the socket, then join.
 * @details The shutdown sequence must be:
 *          1. Set _stop = true so recvLoop() knows the recv failure is intentional.
 *          2. shutdown(SHUT_RDWR) to interrupt any blocked recv() call inside
 *             recvLoop(). Without this, join() would block forever on a server that
 *             has stopped sending.
 *          3. close(_sockfd) to release the fd.
 *          4. join() to wait for the recv thread to exit cleanly.
 */
NetworkClient::~NetworkClient()
{
    // Signal the recv thread that any subsequent recv failure is deliberate.
    _stop.store(true);

    if (_sockfd != -1) {
        // ::shutdown with SHUT_RDWR causes any blocked ::recv() in recvLoop() to
        // return 0 (connection closed), which triggers the thread's exit condition.
        ::shutdown(_sockfd, SHUT_RDWR);
        ::close(_sockfd);
        _sockfd = -1;
    }

    // Join the recv thread if it was started. joinable() returns false if the thread
    // was never started (connect() was never called) or was already joined.
    if (_recvThread.joinable()) {
        _recvThread.join();
    }
}

/**
 * @brief Open the socket, complete the handshake, and start the recv thread.
 * @throws NetworkConnectException on any failure before the thread is started.
 */
void NetworkClient::connect()
{
    // ── Step 1: Resolve the hostname ──────────────────────────────────────────
    //
    // getaddrinfo() translates a hostname string (e.g. "localhost") and a port
    // string into a linked list of addrinfo structs, each representing one way to
    // reach that address (IPv4, IPv6, different interfaces). We iterate the list
    // until a socket() + connect() pair succeeds.
    addrinfo hints{};
    hints.ai_family   = AF_UNSPEC;     // Accept IPv4 or IPv6.
    hints.ai_socktype = SOCK_STREAM;   // We want a reliable, ordered byte stream (TCP).

    const std::string portStr = std::to_string(_port);
    addrinfo* rawResult = nullptr;

    const int gaiRet = ::getaddrinfo(_host.c_str(), portStr.c_str(), &hints, &rawResult);
    // RAII wrapper: freeaddrinfo() is called when addrResult goes out of scope.
    std::unique_ptr<addrinfo, AddrInfoDeleter> addrResult(rawResult);

    if (gaiRet != 0) {
        throw NetworkConnectException(
            "getaddrinfo failed for " + _host + ":" + portStr
            + " — " + ::gai_strerror(gaiRet));
    }

    // ── Step 2: Try each resolved address ─────────────────────────────────────
    //
    // socket() creates a new socket file descriptor. It allocates kernel-side
    // resources (send/receive buffers, a port binding slot) and returns an integer
    // handle (the fd). If creation fails, we try the next candidate address.
    //
    // ::connect() sends the TCP SYN packet and blocks until the three-way handshake
    // (SYN / SYN-ACK / ACK) completes or the attempt times out. After it returns 0,
    // the TCP connection is established and we can send/recv.
    for (addrinfo* rp = addrResult.get(); rp != nullptr; rp = rp->ai_next) {
        const int fd = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd == -1) {
            // socket() failed for this address family — try the next one.
            continue;
        }

        if (::connect(fd, rp->ai_addr, rp->ai_addrlen) == 0) {
            // Connected successfully; store the fd and stop iterating.
            _sockfd = fd;
            break;
        }

        // connect() failed — close this fd and try the next address.
        ::close(fd);
    }

    if (_sockfd == -1) {
        throw NetworkConnectException(
            "Could not connect to " + _host + ":" + portStr
            + " — " + std::strerror(errno));
    }

    spdlog::info("NetworkClient: TCP connected to {}:{}", _host, _port);

    // ── Step 3: Read "WELCOME\n" ──────────────────────────────────────────────
    //
    // The server sends "WELCOME\n" as the first thing after the TCP handshake.
    // Read byte-by-byte until '\n'.
    std::string welcome;
    while (true) {
        char ch = 0;
        // ::recv() copies up to the requested number of bytes from the kernel's socket
        // receive buffer into our buffer. It blocks if no data is available. Returns
        // the number of bytes actually read, 0 on EOF, or -1 on error.
        const ssize_t n = ::recv(_sockfd, &ch, 1, 0);
        if (n <= 0) {
            ::close(_sockfd);
            _sockfd = -1;
            throw NetworkConnectException("Connection closed before WELCOME was received");
        }
        if (ch == '\n') {
            break;
        }
        welcome += ch;
    }

    if (welcome != "WELCOME") {
        ::close(_sockfd);
        _sockfd = -1;
        throw NetworkConnectException("Expected WELCOME, got: " + welcome);
    }

    spdlog::info("NetworkClient: received WELCOME");

    // ── Step 4: Send "GRAPHIC\n" ──────────────────────────────────────────────
    //
    // The protocol requires the GUI client to identify itself by sending "GRAPHIC\n".
    sendRaw("GRAPHIC\n");
    spdlog::info("NetworkClient: sent GRAPHIC");

    // ── Step 5: Send bootstrap queries ───────────────────────────────────────
    sendBootstrap();

    // ── Step 6: Start the receive thread ─────────────────────────────────────
    _recvThread = std::thread([this] { recvLoop(); });

    spdlog::info("NetworkClient: recv thread started");
}

/**
 * @brief Send the four initial bootstrap queries to fill the queue with world state.
 */
void NetworkClient::sendBootstrap()
{
    // msz: request map dimensions → server replies with "msz X Y\n"
    sendRaw("msz\n");
    // mct: request all tile contents → server replies with one "bct x y r0…r6\n" per tile
    sendRaw("mct\n");
    // tna: request team names → server replies with one "tna TeamName\n" per team
    sendRaw("tna\n");
    // sgt: request current time unit → server replies with "sgt t\n"
    sendRaw("sgt\n");

    spdlog::info("NetworkClient: sent bootstrap queries (msz, mct, tna, sgt)");
}

/**
 * @brief Send all bytes of cmd to the server, looping until fully written.
 * @throws NetworkConnectException on fatal send error.
 */
void NetworkClient::sendRaw(std::string_view cmd)
{
    // Acquire the send mutex; std::lock_guard releases it when lock goes out of scope.
    std::lock_guard<std::mutex> lock(_sendMutex);

    const char*  data      = cmd.data();
    std::size_t  remaining = cmd.size();

    while (remaining > 0) {
        // MSG_NOSIGNAL suppresses SIGPIPE: send() returns -1 with errno=EPIPE on disconnect.
        const ssize_t sent = ::send(_sockfd, data, remaining, MSG_NOSIGNAL);
        if (sent < 0) {
            throw NetworkConnectException(
                std::string("send() failed: ") + std::strerror(errno));
        }
        data      += static_cast<std::size_t>(sent);
        remaining -= static_cast<std::size_t>(sent);
    }
}

/**
 * @brief Receive thread entry point: read bytes, split on '\n', parse, push to queue.
 */
void NetworkClient::recvLoop()
{
    constexpr std::size_t BUF_SIZE = 4096;
    char buf[BUF_SIZE];

    while (!_stop.load()) {
        const ssize_t n = ::recv(_sockfd, buf, BUF_SIZE, 0);

        if (n < 0) {
            // EINTR: signal interrupted recv(); retry.
            if (errno == EINTR) {
                continue;
            }
            // Any other error while we are not stopping is a real disconnect.
            if (!_stop.load()) {
                spdlog::error("NetworkClient: recv() error: {}", std::strerror(errno));
                _hadError.store(true);
            }
            break;
        }

        if (n == 0) {
            // The server closed the connection gracefully (EOF / FIN).
            if (!_stop.load()) {
                spdlog::warn("NetworkClient: server closed the connection");
                _hadError.store(true);
            }
            break;
        }

        // Append new bytes to the line accumulator.
        _recvBuf.append(buf, static_cast<std::size_t>(n));

        // Extract and process all complete lines (terminated by '\n').
        std::size_t pos = 0;
        while ((pos = _recvBuf.find('\n')) != std::string::npos) {
            // Extract everything before the '\n' as one complete line.
            std::string line = _recvBuf.substr(0, pos);

            _recvBuf.erase(0, pos + 1);

            if (line.empty()) {
                continue;
            }

            // Parse the line and push the result onto the queue for the main thread.
            try {
                if (auto msg = parseLine(line)) {
                    _queue.push(std::move(*msg));
                }
            } catch (const NetworkParseException& e) {
                // A parse error on one line is not fatal — log and continue.
                // The recv thread must keep running even if a single line is malformed.
                spdlog::warn("NetworkClient: parse error on line '{}': {}", line, e.what());
            }
        }
    }
}

/**
 * @brief Drain one message from the queue, or throw if the connection is lost.
 * @return Next ServerMessage or std::nullopt if the queue is empty.
 * @throws NetworkRecvException if the recv thread flagged a disconnect.
 */
std::optional<ServerMessage> NetworkClient::poll()
{
    if (_hadError.load() && !_stop.load()) {
        throw NetworkRecvException("server disconnected unexpectedly");
    }
    return _queue.tryPop();
}

/**
 * @brief Return true if the recv thread detected an unexpected disconnect.
 * @return true if _hadError is set and the shutdown was not initiated by the destructor.
 */
bool NetworkClient::hasError() const noexcept
{
    return _hadError.load() && !_stop.load();
}

/**
 * @brief Parse one protocol line into a ServerMessage.
 * @throws NetworkParseException on known prefixes with malformed fields.
 */
std::optional<ServerMessage> NetworkClient::parseLine(std::string_view line)
{
    std::string        lineStr(line);
    std::istringstream ss(lineStr);
    std::string        cmd;

    if (!(ss >> cmd)) {
        return std::nullopt;
    }

    // Local lambda: read a '#'-prefixed ID token from the stream.
    auto readIdFromStream = [&](uint32_t& out) {
        std::string tok;
        if (!(ss >> tok)) {
            throw NetworkParseException("Missing ID token in: " + std::string(line));
        }
        out = readId(tok);
    };

    // ── Map size ─────────────────────────────────────────────────────────────
    if (cmd == "msz") {
        MsgMapSize m{};
        if (!(ss >> m.x >> m.y)) {
            throw NetworkParseException("Malformed msz: " + std::string(line));
        }
        return m;
    }

    // ── Tile content ─────────────────────────────────────────────────────────
    if (cmd == "bct") {
        MsgTileContent m{};
        if (!(ss >> m.x >> m.y)) {
            throw NetworkParseException("Malformed bct header: " + std::string(line));
        }
        readResources(ss, m.resources);
        return m;
    }

    // ── Team name ─────────────────────────────────────────────────────────────
    if (cmd == "tna") {
        MsgTeamName m{};
        if (!(ss >> m.name)) {
            throw NetworkParseException("Malformed tna: " + std::string(line));
        }
        return m;
    }

    // ── New player ────────────────────────────────────────────────────────────
    if (cmd == "pnw") {
        MsgPlayerNew m{};
        readIdFromStream(m.id);
        uint32_t orientation = 0;
        uint32_t level = 0;
        if (!(ss >> m.x >> m.y >> orientation >> level >> m.team)) {
            throw NetworkParseException("Malformed pnw: " + std::string(line));
        }
        m.orientation = static_cast<uint8_t>(orientation);
        m.level       = static_cast<uint8_t>(level);
        return m;
    }

    // ── Player position ───────────────────────────────────────────────────────
    if (cmd == "ppo") {
        MsgPlayerPosition m{};
        readIdFromStream(m.id);
        uint32_t orientation = 0;
        if (!(ss >> m.x >> m.y >> orientation)) {
            throw NetworkParseException("Malformed ppo: " + std::string(line));
        }
        m.orientation = static_cast<uint8_t>(orientation);
        return m;
    }

    // ── Player level ──────────────────────────────────────────────────────────
    if (cmd == "plv") {
        MsgPlayerLevel m{};
        readIdFromStream(m.id);
        uint32_t level = 0;
        if (!(ss >> level)) {
            throw NetworkParseException("Malformed plv: " + std::string(line));
        }
        m.level = static_cast<uint8_t>(level);
        return m;
    }

    // ── Player inventory ──────────────────────────────────────────────────────
    if (cmd == "pin") {
        MsgPlayerInventory m{};
        readIdFromStream(m.id);
        if (!(ss >> m.x >> m.y)) {
            throw NetworkParseException("Malformed pin header: " + std::string(line));
        }
        readResources(ss, m.resources);
        return m;
    }

    // ── Player expulsion ──────────────────────────────────────────────────────
    if (cmd == "pex") {
        MsgPlayerExpulsion m{};
        readIdFromStream(m.id);
        return m;
    }

    // ── Player broadcast ──────────────────────────────────────────────────────
    if (cmd == "pbc") {
        MsgPlayerBroadcast m{};
        readIdFromStream(m.id);
        // The message text may contain spaces; std::getline reads the rest of the line.
        // We skip any leading whitespace between the ID and the message with std::ws.
        std::getline(ss >> std::ws, m.message);
        return m;
    }

    // ── Player death ──────────────────────────────────────────────────────────
    if (cmd == "pdi") {
        MsgPlayerDeath m{};
        readIdFromStream(m.id);
        return m;
    }

    // ── Incantation start ─────────────────────────────────────────────────────
    if (cmd == "pic") {
        MsgIncantationStart m{};
        uint32_t level = 0;
        if (!(ss >> m.x >> m.y >> level)) {
            throw NetworkParseException("Malformed pic header: " + std::string(line));
        }
        m.level = static_cast<uint8_t>(level);
        // Read variable number of participating player IDs.
        std::string tok;
        while (ss >> tok) {
            m.players.push_back(readId(tok));
        }
        if (m.players.empty()) {
            throw NetworkParseException("pic with no players: " + std::string(line));
        }
        return m;
    }

    // ── Incantation end ───────────────────────────────────────────────────────
    if (cmd == "pie") {
        MsgIncantationEnd m{};
        std::string result;
        if (!(ss >> m.x >> m.y >> result)) {
            throw NetworkParseException("Malformed pie: " + std::string(line));
        }
        if (result == "ok") {
            m.result = true;
        } else if (result == "ko") {
            m.result = false;
        } else {
            throw NetworkParseException("pie result must be 'ok' or 'ko': " + std::string(line));
        }
        return m;
    }

    // ── Egg laying start ──────────────────────────────────────────────────────
    if (cmd == "pfk") {
        MsgEggLaying m{};
        readIdFromStream(m.playerId);
        return m;
    }

    // ── Resource drop ─────────────────────────────────────────────────────────
    if (cmd == "pdr") {
        MsgResourceDrop m{};
        readIdFromStream(m.playerId);
        uint32_t res = 0;
        if (!(ss >> res)) {
            throw NetworkParseException("Malformed pdr: " + std::string(line));
        }
        m.resource = static_cast<uint8_t>(res);
        return m;
    }

    // ── Resource collect ──────────────────────────────────────────────────────
    if (cmd == "pgt") {
        MsgResourceCollect m{};
        readIdFromStream(m.playerId);
        uint32_t res = 0;
        if (!(ss >> res)) {
            throw NetworkParseException("Malformed pgt: " + std::string(line));
        }
        m.resource = static_cast<uint8_t>(res);
        return m;
    }

    // ── Egg laid at position ──────────────────────────────────────────────────
    if (cmd == "enw") {
        MsgEggLaid m{};
        readIdFromStream(m.eggId);
        readIdFromStream(m.playerId);
        if (!(ss >> m.x >> m.y)) {
            throw NetworkParseException("Malformed enw: " + std::string(line));
        }
        return m;
    }

    // ── Player connected via egg ──────────────────────────────────────────────
    if (cmd == "ebo") {
        MsgEggConnection m{};
        readIdFromStream(m.eggId);
        return m;
    }

    // ── Egg death ─────────────────────────────────────────────────────────────
    if (cmd == "edi") {
        MsgEggDeath m{};
        readIdFromStream(m.eggId);
        return m;
    }

    // ── Time unit query response ──────────────────────────────────────────────
    if (cmd == "sgt") {
        MsgTimeUnit m{};
        if (!(ss >> m.t)) {
            throw NetworkParseException("Malformed sgt: " + std::string(line));
        }
        return m;
    }

    // ── Time unit changed ─────────────────────────────────────────────────────
    if (cmd == "sst") {
        MsgTimeUnitSet m{};
        if (!(ss >> m.t)) {
            throw NetworkParseException("Malformed sst: " + std::string(line));
        }
        return m;
    }

    // ── End of game ───────────────────────────────────────────────────────────
    if (cmd == "seg") {
        MsgEndGame m{};
        if (!(ss >> m.team)) {
            throw NetworkParseException("Malformed seg: " + std::string(line));
        }
        return m;
    }

    // ── Server message ────────────────────────────────────────────────────────
    if (cmd == "smg") {
        MsgServerMessage m{};
        // Free-form text; may contain spaces. Read the rest of the line.
        std::getline(ss >> std::ws, m.message);
        return m;
    }

    // ── Unknown command ───────────────────────────────────────────────────────
    if (cmd == "suc") {
        return MsgUnknownCommand{};
    }

    // ── Bad parameter ─────────────────────────────────────────────────────────
    if (cmd == "sbp") {
        return MsgBadParameter{};
    }

    // Unknown command prefix: log a warning and return nullopt.
    spdlog::warn("NetworkClient: unknown protocol command: '{}'", cmd);
    return std::nullopt;
}
