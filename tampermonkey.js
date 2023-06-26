// ==UserScript==
// @name         ESP32 GPIO Control
// @namespace    http://tampermonkey.net/
// @version      0.1
// @description  Send AJAX request to ESP32
// @author       You
// @match        https://alkoteket.azzar.net/drinkview?*
// @grant        none
// @run-at       document-start
// ==/UserScript==

window.addEventListener('DOMContentLoaded', function() {
    'use strict';

    var id = window.location.search.split("?")[1];
    console.log(id);
    console.log("Tampermonkey ID");
    let fetchedDrink = null;
    let fetchedIngredients = null;
    let validDrink = false;

    fetch("/alkoteket/drink/" + id, {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify({}) // Empty JSON object
    })
        .then(response => {
        if (!response.ok) {
            throw new Error(`HTTP error! status: ${response.status}`);
        }
        return response.json();
    })
        .then(data => {
        fetchedDrink = JSON.parse(data.result); // Parse the JSON string into an object
        fetchedIngredients = fetchedDrink.ingredients;
        console.log(fetchedIngredients);
        console.log(fetchedDrink);
        console.log(JSON.stringify(fetchedDrink));
        console.log("Request");
    })
        .catch(error => console.error('Error:', error));

    function buildESP32Request(){
        let pumps = ['pump1', 'pump2', 'pump3', 'pump4'];
        let requestString = "";
        let localStorageValues = JSON.parse(localStorage.getItem('formValues'));
        let foundIngredients = 0;
        let totalVolume = 0;

        fetchedIngredients.forEach(ingredient => {
            for (let pump = 0; pump < pumps.length; pump++) {
                // Ignore "empty" pumps and pumps not assigned to an ingredient
                if(localStorageValues[pumps[pump]] == ingredient.id && localStorageValues[pumps[pump]] != "empty"){
                    requestString += (pump + 1)+ "=" + ingredient.qty + "&"
                    totalVolume += parseFloat(ingredient.qty);
                    foundIngredients++;
                }
            }
        })
        if(totalVolume > parseFloat(localStorageValues.container_volume)){
            alert("The total volume of ingredients exceeds the container volume.");
            return "";
        }
        if(requestString != "" && fetchedIngredients.length == foundIngredients){
            return requestString.slice(0, -1);
        }
        else{
            alert("Not all ingredients found in the assigned pumps.");
            return "";
        }
    }

    // Function to send Ajax request
    function sendRequest(gpioValues) {
        let ip = null;
        if(localStorage.getItem('formValues')) {
            let values = JSON.parse(localStorage.getItem('formValues'));
            ip = values.ip;
        }
        if(ip != null){
            var xhr = new XMLHttpRequest();
            xhr.open("POST", 'https://' + ip + '/set_gpio?' + gpioValues, true);
        }
        xhr.send();
    }

    // Function to create a button on the page
    function createButton() {
        var button = document.createElement("button");
        button.innerHTML = "Send Request to ESP32";
        button.style = "position:fixed;top:20px;right:20px;padding:10px;";

        button.addEventListener ("click", function() {
            var gpioValues = buildESP32Request();
            if(gpioValues != ""){
                sendRequest(gpioValues);
            }
        });

        document.body.appendChild(button);
    }

    // Create the button on page load
    createButton();

    window.addEventListener('load', function() {
        let button = document.createElement("button");
        button.innerHTML = "Show Form";
        button.onclick = toggleForm;

        let topMenu = document.querySelector("#top_menu");
        if(topMenu) {
            topMenu.appendChild(button);
        }

        let formHtml = `
          <form id="myForm" style="display: none;">
             IP-address: <input type="text" id="ip" name="ip" required><br>
             Container Volume (cl): <input type="number" id="container_volume" name="container_volume" value=0 required><br>
             Pump 1: <select id="pump1" name="pump1"></select><br>
             Pump 2: <select id="pump2" name="pump2"></select><br>
             Pump 3: <select id="pump3" name="pump3"></select><br>
             Pump 4: <select id="pump4" name="pump4"></select><br>
             <input type="submit" value="Submit">
          </form>
         `;

        let mainElement = document.querySelector('main');
        if(mainElement) {
            mainElement.insertAdjacentHTML('afterbegin', formHtml);
        }

        document.getElementById('myForm').onsubmit = function(e) {
              e.preventDefault();
              let formValues = {
                  ip: document.getElementById('ip').value,
                  container_volume: document.getElementById('container_volume').value,
                  pump1: document.getElementById('pump1').value,
                  pump2: document.getElementById('pump2').value,
                  pump3: document.getElementById('pump3').value,
                  pump4: document.getElementById('pump4').value
              };
              localStorage.setItem('formValues', JSON.stringify(formValues));
          };

        fetchIngredients();
    });

    function toggleForm() {
        let form = document.getElementById('myForm');
        if(form.style.display === "none") {
            form.style.display = "block";
        } else {
            form.style.display = "none";
        }
    }

    function fillForm(values) {
        document.getElementById('ip').value = values.ip;
        document.getElementById('container_volume').value = values.container_volume;
        document.getElementById('pump1').value = values.pump1 || "empty";
        document.getElementById('pump2').value = values.pump2 || "empty";
        document.getElementById('pump3').value = values.pump3 || "empty";
        document.getElementById('pump4').value = values.pump4 || "empty";
    }

    function fetchIngredients() {
        fetch('/alkoteket/ingredients?limit=100')
            .then(response => response.json())
            .then(data => {
            let selectFields = ['pump1', 'pump2', 'pump3', 'pump4'];
            selectFields.forEach(field => {
                let selectField = document.getElementById(field);

                // Add the "Empty" option
                let emptyOption = document.createElement("option");
                emptyOption.value = "empty";
                emptyOption.text = "Empty";
                selectField.add(emptyOption);

                data.forEach(item => {
                    let option = document.createElement("option");
                    option.value = item.id;
                    option.text = item.name;
                    selectField.add(option);
                });
            });

            // Load saved form values after fetching ingredients
            if(localStorage.getItem('formValues')) {
                let values = JSON.parse(localStorage.getItem('formValues'));
                fillForm(values);
            }
        });
    }

});