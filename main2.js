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





