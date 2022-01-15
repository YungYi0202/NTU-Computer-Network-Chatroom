var curFriend='Authors';
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
            loadUserFriends(resJson);
        } else {
            usernameInput.value = "";
        }
    } catch(err) {
        console.error(`Error: ${err}`);
    }
  });

/** End Login **/
const historyContainer = document.getElementById('chat-history-container');
const chatWith = document.getElementById('chat-with');
const chatNum = document.getElementById('chat-num-messages');
const friendsSet = new Set();
const friendListContainer = document.getElementById('friend-list');
const fileInput = document.getElementById('file-input');


function createFriendListItem(friendname) {
    const item = document.createElement('li');
    item.setAttribute('class', 'clearfix');

    const div = document.createElement('div');
    div.setAttribute('class', 'name');
    div.innerHTML = friendname;
    
    const deleteBtn = document.createElement('button');
    deleteBtn.innerHTML = 'delete';
    deleteBtn.addEventListener('click', async _ => {
        try {   
            var msg = `delete=${friendname}`;
            await post(msg, 'text/plain');
            friendListContainer.removeChild(item);
        } catch(err) {
            console.error(`Error: ${err}`);
        }
        });
    
    item.appendChild(div);
    item.appendChild(deleteBtn);
    div.addEventListener('click', async _ => {
        try {   
            curFriend = friendname;
            await refreshHistory(friendname);
            refreshHistoryBtn.removeAttribute('disabled');
            messageSendBtn.removeAttribute('disabled');
            fileInput.removeAttribute('disabled');
        } catch(err) {
            console.error(`Error: ${err}`);
        };
    });
    return item;
}

function loadUserFriends(friends) {
    for (var i = 0 ; i < friends.length; i++) {
        const item = createFriendListItem(friends[i]);
        friendsSet.add(friends[i]);
        friendListContainer.appendChild(item);
    }
}

const messageInput = document.getElementById('message-input');
messageInput.addEventListener('keypress', async e => {
    try{
        if (e.key == 'Enter') {
            if (messageInput.value.length > 0) {
                await sendMessage();
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
            await sendMessage();
        }
        messageInput.value='';    
    }
    catch (err) {
        console.error(`Error: ${err}`);
    }
});

async function sendMessage() {
    var msg = `say=${curFriend}=${messageInput.value}`;
    await post(msg, 'text/plain');
    await refreshHistory(curFriend);
}

async function post(msg, type) {
    var xhr = new XMLHttpRequest();
    xhr.open("POST", location.href, true);
    xhr.setRequestHeader('Content-Type', type);
    xhr.send(msg);
}

fileInput.addEventListener('change', async (event) => {
    const file = event.target.files[0];  
    const filename = file.name;
    const type = file.type;
    var reader = new FileReader();
    reader.onload = async function(event) {
        var msg = `${curFriend}=${filename}\r\n${event.target.result}`;
        await post(msg, type);
        await refreshHistory(curFriend);
    };
    reader.readAsText(file);
    fileInput.value='';
  });

const friendInput = document.getElementById('friend-input');
friendInput.addEventListener('keypress', async e => {
    try{
        if (e.key == 'Enter') {
            if (friendInput.value.length > 0) {
                await addFriend();
            }
            friendInput.value='';
        } 
    }
    catch (err) {
        console.error(`Error: ${err}`);
    }
});

async function addFriend() {
    var friendname = friendInput.value;
    if (friendsSet.has(friendname) == false) {
        var msg = `add=${friendname}`;
        await post(msg, 'text/plain');
        const item = createFriendListItem(friendname);
        friendsSet.add(friendname);
        friendListContainer.appendChild(item);
    }
}

const addFriendBtn = document.getElementById('add-friend-btn');
addFriendBtn.addEventListener('click', async _ => {
    try{
        if (friendInput.value.length > 0) {
            await addFriend();
        }
        friendInput.value='';    
    }
    catch (err) {
        console.error(`Error: ${err}`);
    }
});

function isImage(filename) {
    var dot = filename.indexOf('.');
    var type = filename.substring(dot + 1);
    if (type === 'jpg' || type === 'jpeg' || type === 'png') {
        return true;
    }
    return false;
}

function createChatMsg(name, content, outcoming) {
    const li = document.createElement('li');
    if (outcoming) {
        li.classList.add('clearfix');
    }
    const div1 = document.createElement('div');
    div1.classList.add('message-data');
    if (outcoming) {
        div1.classList.add('align-right');
    }
    div1.innerHTML = `<span class="message-data-name">${name}</span>`;
    const div2 = document.createElement('div');
    if (outcoming) {
        div2.setAttribute('class','message other-message float-right');
    } else {
        div2.setAttribute('class','message my-message');
    }
    if (typeof(content) === 'object') {
        const filename = content.File;
        if (isImage(filename)) {
            div2.innerHTML = `<a href="${filename}" download><img src="${filename}" alt="${filename}"></a>`;
        } else {
            div2.innerHTML = `<a href="${filename}" download>${filename}</a>`;
        } 
    } {
        div2.innerHTML = content
    }
    li.appendChild(div1);
    li.appendChild(div2);
    return li;
}

async function refreshHistory(friendname) {
    var myRequest = new Request('?history=' + friendname, getInit);
    const response = await fetch(myRequest);
    const resJson = await response.json();
    if (response.ok) {
        _refreshHistory(resJson);
    }
}

function _refreshHistory(histories) {
    historyContainer.innerHTML = '';
    for (const history of histories) {
        const chatMsg = createChatMsg(history.From, history.Content, (history.To == curFriend));
        historyContainer.appendChild(chatMsg);
    }
    chatWith.innerHTML = `Chat with ${curFriend}`;
    chatNum.innerHTML = `already ${histories.length} messages`;
}

const refreshHistoryBtn = document.getElementById('refresh-history-btn');
refreshHistoryBtn.addEventListener('click', async _ => {
    try {   
        await refreshHistory(curFriend);
    } catch(err) {
        console.error(`Error: ${err}`);
    }
  });