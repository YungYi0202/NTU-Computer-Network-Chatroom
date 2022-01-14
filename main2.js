var myHeaders = new Headers();

var getInit = { method: 'GET',
               headers: myHeaders,
               mode: 'cors',
               cache: 'default' };

/** Login **/
const mainWindow = document.getElementById('main-window');
mainWindow.style.display = 'none';

const loginWindow =  document.getElementById('login-window');
const loginBtn = document.getElementById('login-btn');
const usernameInput = document.getElementsByName('username')[0];

loginBtn.addEventListener('click', async _ => {
    try {   
        var username = usernameInput.value;
        var myRequest = new Request('?login=' + username, getInit);
        const response = await fetch(myRequest);
        const resJson = await response.json();
        console.log('Completed!', response);
        if (response.ok) {
            mainWindow.style.display = 'block';
            loginWindow.style.display = 'none';
            loadUserFriends(resJson.Friend);
        } else {
            usernameInput.value = "";
        }
    } catch(err) {
        console.error(`Error: ${err}`);
    }
  });

/** End Login **/

function createFriendListItem(friendname) {
    const item = document.createElement('li');
    item.setAttribute('class', 'clearfix');
    item.innerHTML = '<div class="name"><i class="fa fa-circle online"></i>'+ friendname+'</div>';

    item.addEventListener('click', async _ => {
        try {   
            var myRequest = new Request('?history=' + friendname, getInit);
            const response = await fetch(myRequest);
            const resJson = await response.json();
            console.log('Completed!', response);
        } catch(err) {
            console.error(`Error: ${err}`);
        }
        });
    return item;
}

const friendList = document.getElementById('friend-list');
function loadUserFriends(friends) {
    
    for (var i = 0 ; i < friends.length; i++) {
        const item = createFriendListItem(friends[i]);
        friendList.appendChild(item);
    }
}

var curFriend='Alice';
const messageInput = document.getElementById('message-input');
messageInput.addEventListener('keypress', async e => {
    try{
        if (e.key == 'Enter') {
            if (messageInput.value.length > 0) {
                sendMessage();
            }
            messageInput.value='';
        } 
    }
    catch (err) {
        console.error(`Error: ${err}`);
    }
});

const messageSendBtn = document.getElementById('message-send-btn');
messageSendBtn.addEventListener('click', async _ => {
    try{
        if (messageInput.value.length > 0) {
            sendMessage();
        }
        messageInput.value='';    
    }
    catch (err) {
        console.error(`Error: ${err}`);
    }
});

async function sendMessage() {
    var msg = `say=${curFriend}=${messageInput.value}`;
    post(msg, 'text/plain');
}

async function post(msg, type) {
    var xhr = new XMLHttpRequest();
    xhr.open("POST", location.href, true);
    xhr.setRequestHeader('Content-Type', type);
    xhr.send(msg);
}

const fileInput = document.getElementById('file-input');
fileInput.addEventListener('change', async (event) => {
    const file = event.target.files[0];  
    const filename = file.name;
    const type = file.type;
    var reader = new FileReader();
    reader.onload = function(event) {
        var msg = `${curFriend}=${filename}\r\n${event.target.result}`;
        post(msg, type);
    };
    reader.readAsText(file);
    fileInput.value='';
  });

