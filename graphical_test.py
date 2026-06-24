##
## EPITECH PROJECT, 2026
## Zappy
## File description:
## graphical_test
##

#this is a test file for reading the official zappy server protocol to the gui

import socket
import time

def test_graphical(socket_file):
    """
    Test the graphical interface functionality.
    test_graphical takes as input the socket file
    it loops (polling) to read the socket file and print all the responses from the server to a file called "graphical_test_output.txt"
    """

    while True:
        # read the socket file
        response = socket_file.recv(1024).decode()
        if not response:
            break
        #with the server befor ai, we launch the server, then this fake gui, and then the ai to see if there is any diiference to
        # launching the server, then the ai, and then the fake gui
        #for the my_sever ones, i am using my server indtead of the provided one.
        with open("gui_output_tests/my_server_before_players.txt", "a") as f:
            f.write(response)
        time.sleep(0.01)


def main():
    """
    here we open port 4242 and we connect to the server, we send the command "GRAPHIC" to the server after we receive the "WELCOME" message, and we check if the server responds with the "GRAPHIC" message.
    """
    # open port 4242 and connect to the server
    import socket
    import time

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect(('localhost', 4242))

    # receive the "WELCOME" message from the server
    welcome_message = sock.recv(1024).decode()

    print(f"Received: {welcome_message}")
    sock.send(b"GRAPHIC\n")
    # now we use test graphical func to prin t in a file all responses from the server
    test_graphical(sock)

if __name__ == "__main__":
    main()