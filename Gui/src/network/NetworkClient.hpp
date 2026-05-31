/**
 * @file network/NetworkClient.hpp
 * @brief TCP connection to the zappy_server — declaration.
 * @details Responsibility: own the POSIX socket file descriptor, manage the background
 *          receive thread, and expose a poll() method that the main thread calls once
 *          per frame to drain parsed protocol messages.
 *
 *          Architecture:
 *            main() constructs one NetworkClient and calls connect().
 *            connect() opens the socket, completes the WELCOME/GRAPHIC handshake,
 *            sends bootstrap queries, and spawns the recv thread.
 *            recvLoop() (recv thread): calls %recv(), accumulates bytes in _recvBuf,
 *            splits on '\n', parses each line with parseLine(), and pushes the result
 *            to _queue.
 *            poll() (main thread): drains one item from _queue per call; throws
 *            NetworkRecvException if the recv thread set _hadError.
 *
 *          Thread model: two threads share _sockfd and _queue.
 *            - _sendMutex serialises all %send() calls.
 *            - _queue (MessageQueue) is internally mutex-protected.
 *            - _stop and _hadError are std::atomic<bool>: written by one thread,
 *              read by another, with no lock required.
 */

#pragma once

#include <atomic>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <mutex>

#include "network/MessageQueue.hpp"
#include "network/ServerMessage.hpp"

/**
 * @brief Owns the TCP connection to the zappy_server.
 * @details Lifetime: created once in main(), connect() called immediately after.
 *          Destroyed when main() returns; the destructor joins the recv thread.
 *
 *          Non-copyable and non-movable by design. There is exactly one server
 *          connection per process, and moving a live socket + thread is unsafe
 *          without additional synchronisation that would add complexity for no gain.
 */
class NetworkClient {
public:
    /**
     * @brief Construct with the server address and port parsed from argv.
     * @details Does NOT open the socket. Call connect() to establish the connection.
     * @param host Hostname or IP address of the zappy_server (e.g. "localhost").
     * @param port TCP port the server is listening on (e.g. 4242).
     */
    explicit NetworkClient(std::string host, uint16_t port);

    /**
     * @brief Destructor: signals the recv thread to stop, closes the socket, then joins.
     * @details The shutdown sequence must happen in this exact order:
     *
     *          1. Set `_stop = true` so recvLoop() knows any subsequent recv failure
     *             is intentional (not a real disconnect).
     *          2. Call `shutdown(SHUT_RDWR)` on the socket. This tells the kernel to
     *             immediately unblock any `recv()` call that is waiting inside
     *             recvLoop(). Without this step, join() would block forever on a
     *             server that has stopped sending.
     *          3. Call `close()` to release the file descriptor.
     *          4. Call `join()` to wait for the recv thread to exit cleanly.
     */
    ~NetworkClient();

    NetworkClient(const NetworkClient&)            = delete; ///< Non-copyable: one connection per process.
    NetworkClient& operator=(const NetworkClient&) = delete; ///< Non-copyable: one connection per process.
    NetworkClient(NetworkClient&&)                 = delete; ///< Non-movable: live socket + thread not movable.
    NetworkClient& operator=(NetworkClient&&)      = delete; ///< Non-movable: live socket + thread not movable.

    /**
     * @brief Open socket, complete WELCOME/GRAPHIC handshake, send bootstrap, start recv thread.
     * @details Executes the following steps in order:
     *
     *          **Step 1 — Hostname resolution:**
     *          `getaddrinfo()` translates the hostname string (e.g. "localhost") into
     *          a linked list of `addrinfo` structs representing every way to reach
     *          that address (IPv4, IPv6, different interfaces). The returned list is
     *          immediately wrapped in a `std::unique_ptr` with a custom deleter so
     *          `freeaddrinfo()` is called automatically on every exit path.
     *
     *          **Step 2 — Socket creation and TCP connect:**
     *          `socket()` allocates a new socket in the kernel and returns an integer
     *          file descriptor. `connect()` sends the TCP SYN packet and blocks until
     *          the three-way handshake (SYN → SYN-ACK → ACK) completes. We iterate
     *          all addresses returned by getaddrinfo() until one succeeds.
     *
     *          **Step 3 — WELCOME handshake:**
     *          The server sends `"WELCOME\n"` immediately after the TCP connection is
     *          established. We read it byte-by-byte (one `recv()` call per character)
     *          to avoid accidentally consuming the first server message after WELCOME
     *          into our buffer.
     *
     *          **Step 4 — GUI identification:**
     *          We send `"GRAPHIC\n"`. This tells the server we are a GUI client, not
     *          an AI player. Without this the server will ignore us.
     *
     *          **Step 5 — Bootstrap queries:**
     *          We immediately send `msz`, `mct`, `tna`, `sgt` so the server starts
     *          streaming the initial world state into the queue.
     *
     *          **Step 6 — Receive thread:**
     *          `std::thread` is constructed with a lambda `[this]{ recvLoop(); }`.
     *          The lambda captures `this` so recvLoop() can access private members.
     *          The thread starts executing immediately.
     *
     * @throws NetworkConnectException if name resolution, socket creation, TCP connect,
     *         or the WELCOME handshake fails.
     */
    void connect();

    /**
     * @brief Drain one parsed message from the internal queue.
     * @details Returns `std::nullopt` when the queue is empty. Typical usage:
     *          @code
     *          while (auto msg = network->poll()) {
     *              std::visit(handler, *msg);
     *          }
     *          @endcode
     *          Internally checks `_hadError` before dequeuing. If the recv thread
     *          detected a disconnect, this method throws instead of returning a value.
     *          This converts the cross-thread atomic flag into a regular C++ exception
     *          on the main thread, where it can be caught normally.
     * @return The next ServerMessage, or std::nullopt if the queue is empty.
     * @throws NetworkRecvException if the recv thread detected a mid-run disconnect.
     */
    std::optional<ServerMessage> poll();

    /**
     * @brief Send a newline-terminated command to the server.
     * @details Protected by `_sendMutex` so calls from the main thread and internal
     *          calls from sendBootstrap() cannot interleave partial writes on the socket.
     *          `std::lock_guard` is RAII: the mutex is released automatically when
     *          `lock` goes out of scope, even if an exception is thrown.
     *
     *          Loops calling `::send()` until all bytes are written. `::send()` copies
     *          bytes into the kernel's TCP send buffer; the kernel handles fragmentation
     *          and retransmission. `MSG_NOSIGNAL` suppresses `SIGPIPE`: if the server
     *          closes the connection, `send()` returns -1 with `errno = EPIPE` instead
     *          of killing the process with a signal.
     *
     * @param cmd String view of the command including the trailing '\n'.
     * @throws NetworkConnectException if `::send()` returns a fatal error.
     */
    void sendRaw(std::string_view cmd);

    /**
     * @brief Check whether the recv thread has flagged a disconnect or error.
     * @details Returns `true` only when `_hadError` is set AND `_stop` is not set.
     *          The `_stop` guard prevents a false positive when the destructor itself
     *          shuts down the socket (which also causes `recv()` to return 0).
     * @return true if the connection was lost unexpectedly.
     */
    [[nodiscard]] bool hasError() const noexcept;

private:
    /**
     * @brief Main function of the receive thread — runs until _stop is set.
     * @details Calls `recv()` in a blocking loop into a 4096-byte stack buffer.
     *          4096 bytes is a typical network MTU multiple — large enough to receive
     *          several protocol lines per call, small enough to live on the stack.
     *
     *          **Why recv() can return a partial line:**
     *          TCP is a byte-stream protocol, not a message protocol. A single `recv()`
     *          call can return anywhere from 1 byte to 4096 bytes regardless of how
     *          many complete lines the server sent. Each new chunk is appended to
     *          `_recvBuf`; complete lines are extracted whenever a `'\n'` is found.
     *          Any incomplete tail stays in `_recvBuf` for the next iteration.
     *
     *          **EINTR handling:**
     *          If a Unix signal (e.g., `SIGWINCH` from a terminal resize) interrupts
     *          `recv()`, it returns -1 with `errno == EINTR`. This is not a real error
     *          — we simply retry the call.
     *
     *          **Error vs. clean shutdown:**
     *          `recv()` returning 0 means the server closed the connection gracefully.
     *          `recv()` returning -1 (non-EINTR) means a socket error. In both cases
     *          we set `_hadError = true` so the main thread is notified via poll().
     *          If `_stop` is already true (destructor in progress), we set no flag —
     *          the shutdown is intentional.
     */
    void recvLoop();

    /**
     * @brief Send the four initial bootstrap queries immediately after the handshake.
     * @details Sends the commands in this order via sendRaw():
     *          - `msz\n` — requests map dimensions; server replies `msz X Y\n`
     *          - `mct\n` — requests all tile contents; server replies with one
     *            `bct X Y q0…q6\n` line per tile
     *          - `tna\n` — requests team names; server replies with one
     *            `tna TeamName\n` line per team
     *          - `sgt\n` — requests the current time unit; server replies `sgt T\n`
     *
     *          These four commands together give the GUI the complete initial snapshot
     *          of the world state before any player events arrive.
     */
    void sendBootstrap();

    /**
     * @brief Parse one complete newline-stripped protocol line into a ServerMessage.
     * @details Dispatches on the first whitespace-delimited token of the line
     *          (e.g., `"msz"`, `"bct"`, `"pnw"`…) and fills the corresponding struct.
     *
     *          **Why std::istringstream:**
     *          `operator>>` on a stream extracts whitespace-separated tokens and
     *          performs type conversion (string → uint32_t, etc.) in one step. Stream
     *          failure is detected by testing the stream state with `if (!(ss >> x))`,
     *          which is cleaner than manual pointer arithmetic or sscanf.
     *
     *          **Player and egg IDs:**
     *          The protocol prefixes numeric IDs with `#` (e.g., `#5`). A local lambda
     *          `readIdFromStream` strips the `#` and calls `std::stoul` to convert.
     *
     *          **Messages with free-form text (pbc, smg):**
     *          After extracting the structured tokens, `std::getline(ss >> std::ws, text)`
     *          reads the rest of the line including any spaces, giving us the full
     *          broadcast or server message text.
     *
     * @param line One complete line from the server, WITHOUT the trailing '\n'.
     * @return A ServerMessage variant on success, or std::nullopt for unknown command
     *         prefixes (which are logged as warnings and silently discarded).
     * @throws NetworkParseException if the line has a known prefix but malformed fields.
     */
    static std::optional<ServerMessage> parseLine(std::string_view line);

    std::string  _host;         ///< Server hostname or IP (stored from constructor).
    uint16_t     _port;         ///< Server TCP port.
    int          _sockfd{-1};   ///< POSIX socket fd; -1 means closed/not yet open.

    std::atomic<bool> _stop{false};    ///< Set by the destructor to signal the recv thread to exit cleanly.

    /// Set by the recv thread when %recv() returns 0 or a fatal error code.
    /// The main thread reads this flag in poll() and converts it to a NetworkRecvException.
    std::atomic<bool> _hadError{false};

    std::thread _recvThread; ///< Background thread running recvLoop().
    std::mutex  _sendMutex;  ///< Serialises all %send() calls on _sockfd.

    MessageQueue<ServerMessage> _queue;   ///< Parsed messages waiting for the main thread.
    std::string                 _recvBuf; ///< Partial-line accumulator for the recv thread.
};
