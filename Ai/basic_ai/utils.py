##
## EPITECH PROJECT, 2026
## Zappy
## File description:
## utils
##


import socket




def move_forward(socket_client: socket.socket) -> bool:
    """
    this is the move forward command that the client will send to the server
    - -->Forward\n
    - <--ok\n
    or
    - <--ko\n
    """
    #print("Moving forward")
    move_forward_message = "Forward\n"
    socket_client.send(move_forward_message.encode())
    #print(f"Sent message to server: {move_forward_message}")
    # then we should receive the OK or KO message from the server
    message = socket_client.recv(1024).decode()
    if not message:
        print("Server closed connection, exiting")
        exit(1)
    if message != "ok\n":
        print(f"moving forward failed, received message from server: {message}")
        return False
    return True

def turn_left(socket_client: socket.socket) -> bool:
    """
    this is the turn left command that the client will send to the server
    - -->Left\n
    - <--ok\n
    or
    - <--ko\n
    """
    #print("Turning left")
    turn_left_message = "Left\n"
    socket_client.send(turn_left_message.encode())
    #print(f"Sent message to server: {turn_left_message}")
    # then we should receive the OK or KO message from the server
    message = socket_client.recv(1024).decode()
    if not message:
        print("Server closed connection, exiting")
        exit(1)
    if message != "ok\n":
        print(f"turning left failed, received message from server: {message}")
        return False
    return True

def turn_right(socket_client: socket.socket) -> bool:
    """
    this is the turn right command that the client will send to the server
    - -->Right\n
    - <--ok\n
    or
    - <--ko\n
    """
    #print("Turning right")
    turn_right_message = "Right\n"
    socket_client.send(turn_right_message.encode())
    #print(f"Sent message to server: {turn_right_message}")
    # then we should receive the OK or KO message from the server
    message = socket_client.recv(1024).decode()
    if not message:
        print("Server closed connection, exiting")
        exit(1)
    if message != "ok\n":
        print(f"turning right failed, received message from server: {message}")
        return False
    return True


def take_object(socket_client: socket.socket, object_name: str) -> bool:
    """
    this is the take object command that the client will send to the server
    - -->Take object_name\n
    - <--ok\n
    or
    - <--ko\n
    """
    #print(f"Taking object: {object_name}")
    take_object_message = f"Take {object_name}\n"
    socket_client.send(take_object_message.encode())
    #print(f"Sent message to server: {take_object_message}")
    # then we should receive the OK or KO message from the server
    message = socket_client.recv(1024).decode()
    if not message:
        print("Server closed connection, exiting")
        exit(1)
    if message != "ok\n":
        print(f"taking object failed, received message from server: {message}")
        return False
    return True

def look(socket_client: socket.socket) -> list[list[str]]:
    """
    this is the look command that the client will send to the server
    - -->Look\n
    - <-- [player, object-on-tile1, ..., object-on-tileP,...]\n (if 3 objects in one tile will be separated by a space)
    or
    - <--ko\n
    if we receive ok, then we should receive a message with the contents of the cell in front of us
    the message will be in the format:
    - <--[object1, object2, object3]\n
    where object1, object2, and object3 are the names of the objects in the cell in front of us
    """
    #print("Looking")
    look_message = "Look\n"
    socket_client.send(look_message.encode())
    #print(f"Sent message to server: {look_message}")
    # then we should receive the OK or KO message from the server
    message = socket_client.recv(2048).decode()
    if not message:
        print("Server closed connection, exiting")
        exit(1)
    if message == "ko\n":
        print(f"looking failed, received message from server: {message}")
        return []

    #now we go through each and every comma, then inside the stuff in betwwen commas we split by space to get the objects on each tile
    #first we remove first [ and last ] and then we split by comma
    message = message[1:-2]
    return_values = []
    tiles = message.split(",")
    for tile in tiles:
        # Split each tile's contents by space to get individual objects
        objects = tile.split()
        return_values.append(objects)
    return return_values


def start_incantation(socket_client: socket.socket) -> int | None:
    """
    this is the start incantation command that the client will send to the server
    - -->Incantation\n
    - <--Elevation underway
Current level: k\n
    or
    - <--ko\n
    """
    #print("Starting incantation")
    incantation_message = "Incantation\n"
    socket_client.send(incantation_message.encode())
    #print(f"Sent message to server: {incantation_message}")
    # then we should receive the OK or KO message from the server
    message = socket_client.recv(1024).decode()
    if not message:
        print("Server closed connection, exiting")
        exit(1)
    if message == "ko\n":
        print(f"incantation failed")
        return None
    #if it not ko, we check if it starts with "Elevation underway"
    if not message.startswith("Elevation underway"):
        print(f"incantation failed, received message from server: {message}")
        return None
    #if it does, we split by newline and get the second line, then we split by space and get the last element which is the current level
    #the incantaion might take very long, so we need to wait for the server to send us the message with the current level, we can do this by looping until we receive a message that starts with "Current level"
    current_level = 0
    while True:
        message = socket_client.recv(1024).decode()
        if not message:
            print("Server closed connection, exiting")
            exit(1)
        if message.startswith("Current level"):
            break
    #print(f"Received succesful message from server: {message}")
    current_level = int(message.split()[-1])

    return current_level

def broadcast_text(socket_client: socket.socket, text: str) -> bool:
    """
    this is the brodcast text command that the client will send to the server
    - -->Broadcast text\n
    - <--ok\n
    or
    - <--ko\n
    """
    #print(f"Brodcating text: {text}")
    brodcast_text_message = f"Broadcast {text}\n"
    socket_client.send(brodcast_text_message.encode())
    #print(f"Sent message to server: {brodcast_text_message}")
    # then we should receive the OK or KO message from the server
    message = socket_client.recv(1024).decode()
    if not message:
        print("Server closed connection, exiting")
        exit(1)
    if message != "ok\n":
        print(f"brodcasting text failed, received message from server: {message}")
        return False
    return True

def get_inventory(socket_client: socket.socket) -> dict[str, int]:
    """
    this is the get inventory command that the client will send to the server
    - -->Inventory\n
    - <--[object1 num1, object2 num2, object3 num3]\n
    or
    - <--ko\n
    """
    #print("Getting inventory")
    get_inventory_message = "Inventory\n"
    socket_client.send(get_inventory_message.encode())
    #print(f"Sent message to server: {get_inventory_message}")
    # then we should receive the OK or KO message from the server
    message = socket_client.recv(2048).decode()
    if not message:
        print("Server closed connection, exiting")
        exit(1)
    if message == "ko\n":
        print(f"getting inventory failed, received message from server: {message}")
        return {}
    #if it not ko, we check if it starts with "["
    if not message.startswith("["):
        print(f"getting inventory failed, received message from server: {message}")
        return {}
    #if it does, we remove the first [ and last ] and then we split by comma to get the individual objects and their counts
    message = message[1:-2]
    inventory = {}
    items = message.split(",")
    for item in items:
        # Split each item by space to get the object name and count
        parts = item.split()
        if len(parts) != 2:
            print(f"getting inventory failed, received message from server: {message}")
            return {}
        object_name = parts[0]
        try:
            object_count = int(parts[1])
        except ValueError:
            print(f"getting inventory failed, received message from server: {message}")
            return {}
        inventory[object_name] = object_count
    return inventory
