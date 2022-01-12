const submitBtn = document.getElementById("submitBtn");
const usernameInput = document.getElementsByName("username");

var myHeaders = new Headers();

var myInit = { method: 'GET',
               headers: myHeaders,
               mode: 'cors',
               cache: 'default' };

submitBtn.addEventListener('click', async _ => {
    try {   
        var username = usernameInput[0].value;
        var myRequest = new Request('?username=' + username, myInit);
        const response = await fetch(myRequest);
        const resJson = await response.json();
        console.log('Completed!', response);
        console.log(response.ok);
        console.log(resJson);
        console.log(resJson.Friend[0])
    } catch(err) {
        console.error(`Error: ${err}`);
    }
  });