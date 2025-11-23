Simon Chaffee, Evan Hill, Max Tsvyetkov


Make sure you have Developer Command Prompt for Visual Studio installed.
If you do not have Developer Command Prompt, go to https://visualstudio.microsoft.com/downloads/ . Scroll down until you see Tools For Visual Studio and click the drop down.
Download Build Tools for Visual Studio.
Complete the download process.

How to run code for part 1:
1. In the start menu, search for Developer Command Prompt for Visual Studio.
2. Navigate to project folder, wherever it is on your system:
    cd C:\whatever\your\path\is\
3. Compile the server code:
    cl /EHsc /std:c++17 server.cpp /Fe:server.exe ws2_32.lib
4. Compile the client code:
    cl /EHsc /std:c++17 client.cpp /Fe:client.exe ws2_32.lib
5. When compiling each you should see:
    server.exe
    client.exe
6. In the server terminal, run the server code using a port number (8000 for this example):
    server.exe 8000
7. You should see the output:
    Server listening on port 8000...
8. Open a seperate Developer Command Prompt window, this will act as the first client
9. Navigate to the project folder again:
    cd C:\whatever\your\path\is\
10. Run a client with the port number you opened with the server:
    client.exe 127.0.0.1 8000
11. Once connected you are able to Login and start posting messages
12. Login to the server using you username (try random name for test)
    LOGIN simon
13. Once logged in, you are now able to use all commands
14. To post a message, use the command POST
    POST Hello|This is a test
15. To see all connected users, use the command USERS
    USERS
16. To leave the message board, use the command LEAVE
    LEAVE
17. To disconnect from the server, use the command QUIT
    QUIT
18. You can connect more than one client to the server. To do so, follow the same steps as the first client but in another Developer Command Prompt window. Each additional client must be connected through a different terminal.
19. If there is more than one user connected, other users will get notified when other users join or post messages.
20. To read the message of another client, use the command MSG followed by the message ID:
    MSG 1