# Computer Network Report

## Team Members and Work Divisions

- B08902029 陳咏誼: client and brwoser
	- `client.cpp, index2.html, index2.css, main2.js`
- B08902071 塗季芸: server, console and database
	- `server.cpp, console.cpp`

## Accomplishment

We satisfy **all requirements** with the following ==**Bonus**==:
- Browser mode:
	- Upload text file through browser.
	- Click image to download image.
	

## README (User Instruction)

- Demo Link: https://youtu.be/QdFPxIA9WrI
- Github Link: https://github.com/jiyuntu/NTU-Computer-Network-Chatroom

### Compilation

```
$ make
$ ./server [port]
$ ./client [ip:port] [port2] // For browser mode
$ ./console [ip:port]	// For console mode
```

### Neccessary Files and Directories

#### Server

```
|   Makefile
|   server.cpp
|   sqlite3.c
|   sqlite3.h
|   sqlite3.o
|   sqlite
|   shell.c
|       
+---server_dir
    |
    +---default/
    	|   index2.html
    	|   main2.js
    	|   index2.css
    	|   report.pdf
```

#### Client (Browser mode)

```
|   Makefile
|   client.cpp
```

#### Client (Console mode)

```
|   Makefile
|   console.cpp
|       
+---client_dir 
    |   any_file_to_be_put
```

### Browser Mode

1. Access `http://localhost:port2/` 
2. Enter `$username` to login
<img src="https://i.imgur.com/1Avymru.png" style="zoom:50%">
3. Start chatting! 
    
You can
- Upload File
- Click image or file link to download file
- Add/Delete friend
- Refresh history
    - Every time user sends a message the history would be refreshed.
- Send message
- Every input section allow uses press `enter` to send.
<img src="https://i.imgur.com/fH15TqZ.png" style="zoom:50%">

### Console Mode
#### 1. Add a new friend

```
add $username $friendname
```

#### 2. Delete a friend

```
delete $username $friendname
```

#### 3. List all friends

```
ls $username

response:
["friend1", "friend2", ...]
```

#### 4. Say something to a specific friend

```
say $username $friendname $something
```

#### 5. Show the chat history with a specify friend

```
history $username friendname

response: 
[{
"From": "$username",
"To": "$friendname",
"Content": "A"
},
{
"From": "$friendname",
"To": "$username",
"Content": {"File": "a.jpg"}	// The friend uploads a file
}]
```

#### 6. Get a file 

```
get $username $filename
```

#### 7. Put a file to a friend

```
put $username $friendname $filename
```
