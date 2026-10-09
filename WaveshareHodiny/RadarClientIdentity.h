#pragma once

class HTTPClient;

// Call from the main task (internal RAM stack), after Wi-Fi is connected.
// Flash writes must never run on the radar worker's PSRAM stack.
bool prepareRadarClientIdentity();

// Call only for the trusted Nebovidy Radar origin, from the radar worker.
// Identity lives outside the portable settings snapshot.
void addRadarClientHeaders(HTTPClient &http);
