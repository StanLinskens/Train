#include <Arduino.h>
#include <M5Unified.h>

// ----------------------
// CONFIGURATION
// ----------------------
const int NUM_STATIONS = 5;    // Total number of stations
const int NUM_SWITCHES = 5;    // Total number of switches

// ----------------------
// SWITCH CONNECTIONS
// ----------------------
// Each switch connects to up to 3 nodes (station or another switch)
// -1 = no connection in that slot
// Example: switchConnections[0] = {0,1,1} means:
// Switch 1 connects to Station1 (0), Station2 (1), and Switch2 (1)
int switchConnections[NUM_SWITCHES][3] = {
  {0, 1, 1},  // Switch 1 -> Station1, Station2, Switch2
  {0, 2, 4},  // Switch 2 -> Switch1, Switch3, Switch5
  {2, 3, 2},  // Switch 3 -> Switch2, Switch4, Station3
  {2, 4, 4},  // Switch 4 -> Switch3, Switch5, Station5
  {1, 3, 3}   // Switch 5 -> Switch2, Switch4, Station4
};

// ----------------------
// STATION TO SWITCH MAP
// ----------------------
// Each station is connected to a specific switch
// This allows BFS to know where to start and where to end
int stationToSwitch[NUM_STATIONS] = {
  0,  // Station1 -> Switch1
  0,  // Station2 -> Switch1
  2,  // Station3 -> Switch3
  4,  // Station4 -> Switch5
  3   // Station5 -> Switch4
};

// ----------------------
// VARIABLES
// ----------------------
int startStation = 0;         // Index of currently selected start station
int endStation = 4;           // Index of currently selected end station
bool selectingStart = true;   // True if user is selecting start, false if end

int route[NUM_SWITCHES];      // Stores the sequence of switches for the route
int routeLength = 0;          // Number of switches in the current route

// ----------------------
// BFS ROUTE FINDING
// ----------------------
bool findSwitchRoute(int start, int end) {
    int startSwitch = stationToSwitch[start];  // Which switch the start station is attached to
    int endSwitch = stationToSwitch[end];      // Which switch the end station is attached to

    bool visited[NUM_SWITCHES] = {false};     // Keep track of visited switches
    int prev[NUM_SWITCHES];                   // Stores the previous switch to reconstruct route
    for (int i = 0; i < NUM_SWITCHES; i++) prev[i] = -1;

    int queue[NUM_SWITCHES];  // BFS queue
    int front = 0, back = 0;

    // Start BFS from the switch attached to the start station
    queue[back++] = startSwitch;
    visited[startSwitch] = true;

    while (front < back) {
        int current = queue[front++];

        // Check if we've reached the switch attached to the end station
        if (current == endSwitch) {
            // Build route backwards
            routeLength = 0;
            int s = current;
            while (s != -1) {
                route[routeLength++] = s;
                s = prev[s];
            }

            // Reverse route so it goes start → end
            for (int i = 0; i < routeLength / 2; i++) {
                int tmp = route[i];
                route[i] = route[routeLength - i - 1];
                route[routeLength - i - 1] = tmp;
            }
            return true; // Route found
        }

        // Visit all neighbors of the current switch
        for (int i = 0; i < 3; i++) {
            int neighbor = switchConnections[current][i];
            if (neighbor == -1) continue;         // Skip empty slots
            if (!visited[neighbor]) {
                queue[back++] = neighbor;         // Add to BFS queue
                visited[neighbor] = true;         // Mark as visited
                prev[neighbor] = current;         // Remember previous switch
            }
        }
    }

    return false; // No route found
}

// ----------------------
// USER INTERFACE
// ----------------------
void drawUI() {
    M5.Display.fillScreen(BLACK);
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(WHITE);
    M5.Display.setCursor(10, 10);

    // Display header
    if (selectingStart) M5.Display.println("Select START Station");
    else M5.Display.println("Select END Station");

    M5.Display.println("-----------------------");

    // Display all stations and highlight current selection
    for (int i = 0; i < NUM_STATIONS; i++) {
        if ((selectingStart && i == startStation) || (!selectingStart && i == endStation))
            M5.Display.setTextColor(GREEN);
        else
            M5.Display.setTextColor(WHITE);

        M5.Display.printf("Station %d\n", i + 1);
    }

    // After selecting end station, show the route
    if (!selectingStart) {
        M5.Display.setTextColor(WHITE);
        M5.Display.println("-----------------------");
        if (findSwitchRoute(startStation, endStation)) {
            M5.Display.println("Switch Route:");
            for (int i = 0; i < routeLength; i++) {
                M5.Display.printf("Switch %d\n", route[i] + 1); // +1 for human-friendly numbering
            }
        } else {
            M5.Display.println("No route available!");
        }
    }

    // Show button instructions
    M5.Display.println("\nA: <-  B: Select  C: ->");
}

// ----------------------
// SETUP
// ----------------------
void setup() {
    M5.begin();
    M5.Display.setRotation(1);
    drawUI();
}

// ----------------------
// MAIN LOOP
// ----------------------
void loop() {
    M5.update();

    // Button A = move selection left
    if (M5.BtnA.wasPressed()) {
        if (selectingStart) startStation = (startStation + NUM_STATIONS - 1) % NUM_STATIONS;
        else endStation = (endStation + NUM_STATIONS - 1) % NUM_STATIONS;
        drawUI();
    }

    // Button C = move selection right
    if (M5.BtnC.wasPressed()) {
        if (selectingStart) startStation = (startStation + 1) % NUM_STATIONS;
        else endStation = (endStation + 1) % NUM_STATIONS;
        drawUI();
    }

    // Button B = toggle between start/end selection
    if (M5.BtnB.wasPressed()) {
        selectingStart = !selectingStart;
        drawUI();
    }
}
