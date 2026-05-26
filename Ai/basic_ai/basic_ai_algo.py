##
## EPITECH PROJECT, 2026
## Zappy
## File description:
## ai_test
##

import sys
import socket



def help_message(return_code: int = 0) -> None:
    print("USAGE: ./zappy_ai -p port -n name -h machine")
    exit(return_code)

def parse_arguments() -> tuple[int, str, str]:
    """
    -p is obligatory, but for now we will assume it is 4242
    -n is obligatory, but for now we will assume it is steve
    -h is not obligatory, default is localhost
    """
    if (len(sys.argv) >= 2 and (sys.argv[1] == "-h" or sys.argv[1] == "--help")):
        help_message(0)

    return_values : tuple

    return_values = (4242, "steve", "localhost")

    return return_values

def handshake_protocol(socket_client: socket.socket, team_name: str) -> None:
    """
    this is the handshake protocol that the client and server will follow
    - <--WELCOME\n
    - -->TEAM-NAME\n
    - <--CLIENT-NUM\n
    - <-- X Y\n

    X and Y indicate the world's dimensions.
    CLIENT-NUM indicates the number of slots available on the server for the TEAM-NAME team.
    If this number is greater than or equal to 1, a new client can connect.
    """
    print("Starting handshake protocol")

    # first we should receive the WELCOME message from the server
    welcome_message = socket_client.recv(1024).decode()
    print(f"Received message from server: {welcome_message}")
    if welcome_message != "WELCOME\n":
        print("Did not receive WELCOME message from server, exiting")
        exit(1)
    # then we should send the TEAM-NAME message to the server
    team_name_message = f"{team_name}\n"
    socket_client.send(team_name_message.encode())
    print(f"Sent message to server: {team_name_message}")
    # then we have a loop where we read messages. each message is separated by \n

    message = socket_client.recv(1024).decode()
    if not message:
        print("Server closed connection, exiting")
        exit(0)
    #now we separate the message by \n and print each line
    messages = message.split("\n")
    for msg in messages:
        if msg:
            print(f"Received message from server: {msg}")

    print(f"number of open slots remaining for team {team_name}: {messages[0]}")
    print(f"world dimensions: {messages[1]}")



def run_ai(socket_client: socket.socket):
    """
    this will be our very basic ai that will attempt to collect as much food as possible to stay alive as long as possible

    now how can we do this?
    """
    pass



def connect_to_server(port: int, team_name: str, machine: str) -> None:
    """
    what needs to happen here:
    client opens socket on server port
    then client server communicate following way:
    - <--WELCOME\n
    - -->TEAM-NAME\n
    - <--CLIENT-NUM\n
    - <-- X Y\n

    X and Y indicate the world's dimensions.
    CLIENT-NUM indicates the number of slots available on the server for the TEAM-NAME team.
    If this number is greater than or equal to 1, a new client can connect.
    """
    print(f"Connecting to server on port {port} with name {team_name} and machine {machine}")

    #first we need to create a socket and connect to the server
    # we use the socket library for this

    socket_client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    socket_client.connect((machine, port))
    print("Connected to server")

    handshake_protocol(socket_client, team_name)





def main():
    return_values = parse_arguments()
    connect_to_server(*return_values)




if __name__ == "__main__":
    main()