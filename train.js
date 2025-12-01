document.addEventListener('DOMContentLoaded', () => {

    // Toggle switch lights between red and green
let switchState = false; // false = red light on, true = green light on

const switchButton = document.querySelector('.switch');

switchButton.addEventListener('click', () => {
    switchState = !switchState;
    
    // Get all switch light containers
    const switches = [
        document.getElementById('switch_light'),
        document.getElementById('switch_light_2'),
        document.getElementById('switch_light_3'),
        document.getElementById('switch_light_4'),
        document.getElementById('switch_light_5')
    ];
    
    // Toggle lights for all switches
    switches.forEach(switchLight => {
        const redLight = switchLight.querySelector('.red_light');
        const greenLight = switchLight.querySelector('.green_light');
        
        if (switchState) {
            // Green light on, red light off
            redLight.style.display = 'none';
            greenLight.style.display = 'block';
        } else {
            // Red light on, green light off
            redLight.style.display = 'block';
            greenLight.style.display = 'none';
        }
    });
    
    // Update button text
    switchButton.textContent = switchState ? 'Switch: GREEN' : 'Switch: RED';
});

// Initialize with red lights on
document.addEventListener('DOMContentLoaded', () => {
    const switches = [
        document.getElementById('switch_light'),
        document.getElementById('switch_light_2'),
        document.getElementById('switch_light_3'),
        document.getElementById('switch_light_4'),
        document.getElementById('switch_light_5')
    ];
    
    switches.forEach(switchLight => {
        const greenLight = switchLight.querySelector('.green_light');
        greenLight.style.display = 'none';
    });
    
    switchButton.textContent = 'Switch: RED';
});



});
