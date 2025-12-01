document.addEventListener('DOMContentLoaded', () => {

    // ================================
    //  SWITCH LIGHT TOGGLE SYSTEM
    // ================================
    let switchState = false; // false = red light on, true = green light on
    const switchButton = document.querySelector('.switch');

    const allSwitches = [
        document.getElementById('switch_light'),
        document.getElementById('switch_light_2'),
        document.getElementById('switch_light_3'),
        document.getElementById('switch_light_4'),
        document.getElementById('switch_light_5')
    ];

    // Toggle lights on click
    switchButton.addEventListener('click', () => {
        switchState = !switchState;

        allSwitches.forEach(switchLight => {
            const redLight = switchLight.querySelector('.red_light');
            const greenLight = switchLight.querySelector('.green_light');

            if (switchState) {
                redLight.style.display = 'none';
                greenLight.style.display = 'block';
            } else {
                redLight.style.display = 'block';
                greenLight.style.display = 'none';
            }
        });

        switchButton.textContent = switchState ? 'Switch: GREEN' : 'Switch: RED';

        // Update "last pressed" text
        pressedInfo.textContent = "Last pressed: SWITCH BUTTON";
    });

    // Initialize on page load with red lights on
    allSwitches.forEach(switchLight => {
        const greenLight = switchLight.querySelector('.green_light');
        greenLight.style.display = 'none';
    });
    switchButton.textContent = 'Switch: RED';



    // ================================
    //  SHOW WHICH BUTTON WAS PRESSED
    // ================================
    const pressedInfo = document.getElementById('pressedInfo');

    const stationButtons = [
        document.getElementById('station1'),
        document.getElementById('station2'),
        document.getElementById('station3'),
        document.getElementById('station4'),
        document.getElementById('station5')
    ];

    // Add click listeners for each station button
    stationButtons.forEach(btn => {
        btn.addEventListener('click', () => {
            pressedInfo.textContent = "Last pressed: " + btn.textContent + " Train is traveling to " + btn.textContent;
        });
    });

});
