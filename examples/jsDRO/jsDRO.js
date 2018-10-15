var ws = null;

function OpenWebsocket(address) {

    ws = new WebSocket(address);

    ws.onopen = function () {
        // ...
        document.getElementById("myDRO").textContent = "Connected";
    };

    ws.onclose = function () {
        // ...
    };

    ws.onerror = function (error) {
        console.log('WebSocket Error: ' + error);
    };

    ws.onmessage = function (event) {
        //console.log('ws data:' + event.data);
        if (event.data[0] == '{') {
            var msg = JSON.parse(event.data);
            document.getElementById("myDRO").textContent = msg.axis0.toFixed(2).replace(/\d(?=(\d{3})+\.)/g, '$&,');
        }
    }
}

function CloseWebsocket() {
    ws.close();
}

window.onload = function() {    
    OpenWebsocket('ws://' + document.getElementById("ip").value);
};
