// BLE Configuration - Nordic UART Service UUIDs
const NUS_SERVICE_UUID = '6e400001-b5a3-f393-e0a9-e50e24dcca9e';
const NUS_RX_CHAR_UUID = '6e400002-b5a3-f393-e0a9-e50e24dcca9e';
const NUS_TX_CHAR_UUID = '6e400003-b5a3-f393-e0a9-e50e24dcca9e';

// Device state
const connectedDevices = {
  train: null,
  switch: null
};

// Track switch states (1-5, true = on, false = off)
const switchStates = {
  1: false,
  2: false,
  3: false,
  4: false,
  5: false
};

// Station coordinates for train animation
const stationCoordinates = {
  1: { x: 420, y: 95 },
  2: { x: 95, y: 175 },
  3: { x: 1195, y: 175 },
  4: { x: 120, y: 475 },
  5: { x: 1050, y: 475 }
};

// Track layout connectivity: which stations can be reached directly from each station
// Based on the track SVG layout with switches
const stationConnectivity = {
  1: { reachable: [2],     switches: ['lols1p0'],                      command: 'M30',  timer: 2000 },
  2: { reachable: [1, 3],  switches: ['lols1p0', 'lols4p1'],          command: 'M30',  timer: 2000 },
  3: { reachable: [2, 4],  switches: ['lols1p1', 'lols4p0'],          command: 'M-30', timer: 2000 },
  4: { reachable: [3, 5],     switches: ['lols1p0', 'lols4p1'],          command: 'M-30', timer: 2000 },
  5: { reachable: [4],     switches: ['lols1p1', 'lols4p0'],          command: 'M30',  timer: 2000 }
};

// Current train state
let currentStation = 2;      // Train starts at station 2
let isMoving = false;        // Track if train is currently moving
let activeTimeout = null;    // Track active movement timeout

// Helper to get timer for a specific route
function getTimer(fromStation, toStation) {
  const input = document.querySelector(`.timer-input[data-from="${fromStation}"][data-to="${toStation}"]`);
  if (input) {
    return parseInt(input.value, 10);
  }
  return 2000; // Default fallback
}

// Update visual switch indicator
function updateSwitchIndicator(switchNumber, isOn) {
  switchStates[switchNumber] = isOn;
  const indicator = document.getElementById(`switchInd${switchNumber}`);
  if (indicator) {
    // Just update the classes - don't modify the content
    indicator.classList.remove('on', 'off');
    indicator.classList.add(isOn ? 'on' : 'off');
    console.log(`Switch ${switchNumber} updated to: ${isOn ? 'ON' : 'OFF'}`);
  }
}

// Update train position on the track
function updateTrainPosition(stationId) {
  const trainIcon = document.getElementById('trainIcon');
  if (!trainIcon || !stationCoordinates[stationId]) return;
  
  const coords = stationCoordinates[stationId];
  trainIcon.style.left = coords.x + 'px';
  trainIcon.style.top = coords.y + 'px';
}

// Animate train movement between stations
async function animateTrainMovement(fromStation, toStation) {
  const trainIcon = document.getElementById('trainIcon');
  if (!trainIcon) return;
  
  const from = stationCoordinates[fromStation];
  const to = stationCoordinates[toStation];
  
  const startTime = Date.now();
  const duration = 500; // 500ms animation
  
  const animate = () => {
    const elapsed = Date.now() - startTime;
    const progress = Math.min(elapsed / duration, 1);
    
    const x = from.x + (to.x - from.x) * progress;
    const y = from.y + (to.y - from.y) * progress;
    
    trainIcon.style.left = x + 'px';
    trainIcon.style.top = y + 'px';
    trainIcon.classList.add('moving');
    
    if (progress < 1) {
      requestAnimationFrame(animate);
    } else {
      trainIcon.classList.remove('moving');
      updateTrainPosition(toStation);
    }
  };
  
  animate();
}

// Highlight current station button
function highlightCurrentStation() {
  for (let i = 1; i <= 5; i++) {
    const btn = document.getElementById(`station${i}`);
    if (btn) {
      if (i === currentStation) {
        // Add the current-station class for gold highlighting
        btn.classList.add('current-station');
      } else {
        // Remove the current-station class from others
        btn.classList.remove('current-station');
      }
    }
  }
}

// Helper: Send command to a device (train or switch)
async function sendCommand(deviceType, command) {
  const device = connectedDevices[deviceType];
  if (!device) {
    console.log(`Device ${deviceType} not connected`);
    return false;
  }

  try {
    const encoder = new TextEncoder();
    const data = encoder.encode(String(command));
    await device.nusRxChar.writeValue(data);
    console.log(`Sent to ${deviceType}: ${command}`);
    
    // Parse switch commands and update indicators
    if (deviceType === 'switch' && command.startsWith('lols')) {
      // Format: lols1p1 = switch 1 ON, lols2p0 = switch 2 OFF
      const match = command.match(/lols(\d)p(\d)/);
      if (match) {
        const switchNum = parseInt(match[1]);
        const isOn = match[2] === '1';
        updateSwitchIndicator(switchNum, isOn);
      }
    }
    
    return true;
  } catch (err) {
    console.error(`Error sending to ${deviceType}:`, err);
    return false;
  }
}

// Connect to a BLE device and store it
async function connectDevice(deviceType) {
  if (!('bluetooth' in navigator)) {
    alert('Web Bluetooth not supported');
    return;
  }

  try {
    console.log(`Requesting ${deviceType} device...`);
    const device = await navigator.bluetooth.requestDevice({
      filters: [
        { namePrefix: 'm5' },
        { namePrefix: 'M5' }
      ],
      acceptAllDevices: false,
      optionalServices: [NUS_SERVICE_UUID],
    });

    console.log(`Connected to ${deviceType}:`, device.name);
    const gattServer = await device.gatt.connect();
    
    const nusService = await gattServer.getPrimaryService(NUS_SERVICE_UUID);
    const nusRxChar = await nusService.getCharacteristic(NUS_RX_CHAR_UUID);
    const nusTxChar = await nusService.getCharacteristic(NUS_TX_CHAR_UUID);

    // Start notifications
    await nusTxChar.startNotifications();
    nusTxChar.addEventListener('characteristicvaluechanged', (event) => {
      const value = event.target.value;
      const text = new TextDecoder().decode(value);
      console.log(`From ${deviceType}:`, text);
    });

    // Handle disconnection
    device.addEventListener('gattserverdisconnected', () => {
      console.log(`${deviceType} device disconnected`);
      connectedDevices[deviceType] = null;
      updateDeviceStatus();
    });

    // Store the connection
    connectedDevices[deviceType] = {
      device,
      gattServer,
      nusRxChar,
      nusTxChar
    };

    updateDeviceStatus();
  } catch (err) {
    console.error(`Error connecting to ${deviceType} device:`, err);
    alert(`Failed to connect ${deviceType} device: ${err.message}`);
  }
}

// Update UI status indicators
function updateDeviceStatus() {
  // Train device status
  const trainDot = document.getElementById('trainDot');
  const trainStatusText = document.getElementById('trainStatusText');
  const trainInfo = document.getElementById('trainDeviceInfo');
  
  if (connectedDevices.train) {
    if (trainDot) {
      trainDot.className = 'status-dot connected';
    }
    if (trainStatusText) {
      trainStatusText.className = 'status-text connected';
      trainStatusText.textContent = 'Connected';
    }
    if (trainInfo) {
      trainInfo.textContent = `${connectedDevices.train.device.name || 'M5 Device'}`;
    }
  } else {
    if (trainDot) {
      trainDot.className = 'status-dot disconnected';
    }
    if (trainStatusText) {
      trainStatusText.className = 'status-text disconnected';
      trainStatusText.textContent = 'Disconnected';
    }
    if (trainInfo) {
      trainInfo.textContent = '';
    }
  }

  // Switch device status
  const switchDot = document.getElementById('switchDot');
  const switchStatusText = document.getElementById('switchStatusText');
  const switchInfo = document.getElementById('switchDeviceInfo');
  
  if (connectedDevices.switch) {
    if (switchDot) {
      switchDot.className = 'status-dot connected';
    }
    if (switchStatusText) {
      switchStatusText.className = 'status-text connected';
      switchStatusText.textContent = 'Connected';
    }
    if (switchInfo) {
      switchInfo.textContent = `${connectedDevices.switch.device.name || 'M5 Device'}`;
    }
  } else {
    if (switchDot) {
      switchDot.className = 'status-dot disconnected';
    }
    if (switchStatusText) {
      switchStatusText.className = 'status-text disconnected';
      switchStatusText.textContent = 'Disconnected';
    }
    if (switchInfo) {
      switchInfo.textContent = '';
    }
  }
}

// Update station button states: enable reachable stations, disable others
function updateStationButtons() {
  const reachable = stationConnectivity[currentStation].reachable;
  
  for (let i = 1; i <= 5; i++) {
    const btn = document.getElementById(`station${i}`);
    if (!btn) continue;

    if (i === currentStation || isMoving) {
      // Disable current station and all during movement
      btn.disabled = true;
      btn.style.opacity = '0.5';
      btn.style.cursor = 'not-allowed';
    } else if (reachable.includes(i)) {
      // Enable reachable stations
      btn.disabled = false;
      btn.style.opacity = '1';
      btn.style.cursor = 'pointer';
    } else {
      // Disable unreachable stations
      btn.disabled = true;
      btn.style.opacity = '0.3';
      btn.style.cursor = 'not-allowed';
    }
  }
}

// Execute a station's automation sequence
async function executeStation(targetStation) {
  // Check if train is already moving
  if (isMoving) {
    console.log('Train is already moving. Wait for it to arrive first.');
    return;
  }

  // Check if target is reachable from current station
  const reachable = stationConnectivity[currentStation].reachable;
  if (!reachable.includes(targetStation)) {
    console.log(`Station ${targetStation} not reachable from station ${currentStation}`);
    console.log(`Reachable stations: ${reachable.join(', ')}`);
    return;
  }

  console.log(`Moving train from station ${currentStation} to station ${targetStation}...`);
  isMoving = true;
  updateStationButtons();

  // Clear any existing timeout
  if (activeTimeout) {
    clearTimeout(activeTimeout);
    activeTimeout = null;
  }

  const config = stationConnectivity[targetStation];
  const timer = getTimer(currentStation, targetStation); // Get custom timer for this route

  // Send all switch commands to configure the path
  for (const switchCmd of config.switches) {
    await sendCommand('switch', switchCmd);
    await new Promise(resolve => setTimeout(resolve, 100)); // Small delay between commands
  }

  // Animate train movement
  await animateTrainMovement(currentStation, targetStation);

  // Send train movement command
  await sendCommand('train', config.command);
  console.log(`Train moving for ${timer}ms to reach station ${targetStation}`);

  // Schedule STOP command and position update after timer
  activeTimeout = setTimeout(async () => {
    console.log(`Train arrived at station ${targetStation}`);
    await sendCommand('train', 'STOP');
    currentStation = targetStation;
    isMoving = false;
    updateStationDisplay();
    updateStationButtons();
    highlightCurrentStation();
    activeTimeout = null;
  }, timer);
}

// Update UI to reflect current station
function updateStationDisplay() {
  const stationNumber = document.getElementById('stationNumber');
  const stationStatusText = document.getElementById('stationStatusText');
  
  if (stationNumber) {
    stationNumber.textContent = currentStation;
  }
  
  if (stationStatusText) {
    if (isMoving) {
      stationStatusText.textContent = `Moving to Station ${currentStation}...`;
    } else {
      stationStatusText.textContent = `Arrived at Station ${currentStation}`;
    }
  }
}

// Initialize on page load
document.addEventListener('DOMContentLoaded', () => {
  console.log('Train page loaded');

  // Initialize train at station 2
  currentStation = 2;
  updateTrainPosition(currentStation);
  updateStationDisplay();
  updateStationButtons();
  highlightCurrentStation();
  
  // Initialize all switch indicators to OFF (red)
  for (let i = 1; i <= 5; i++) {
    updateSwitchIndicator(i, false);
  }

  // Connect button listeners
  document.getElementById('connectTrainBtn').addEventListener('click', () => {
    connectDevice('train');
  });

  document.getElementById('connectSwitchBtn').addEventListener('click', () => {
    connectDevice('switch');
  });

  // Station button listeners - execute automation when clicked
  for (let i = 1; i <= 5; i++) {
    const stationBtn = document.getElementById(`station${i}`);
    if (stationBtn) {
      stationBtn.addEventListener('click', () => {
        if (!isMoving && i !== currentStation) {
          executeStation(i);
        }
      });
    }
  }

  // Toggle switch lights between red and green (visual test feature)
  let switchState = false;
  const switchButton = document.querySelector('.switch');

  switchButton.addEventListener('click', () => {
    switchState = !switchState;

    const switches = [
      document.getElementById('switch_light'),
      document.getElementById('switch_light_2'),
      document.getElementById('switch_light_3'),
      document.getElementById('switch_light_4'),
      document.getElementById('switch_light_5')
    ];

    switches.forEach(switchLight => {
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
  });

  // Initialize switch lights to red
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

  // Update device status on load
  updateDeviceStatus();
});
