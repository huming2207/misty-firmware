# Misty Firmware Configuration API

This document describes the HTTP API provided by the Misty smart irrigation pump firmware for configuration and control.

## Schedule Management

### List All Schedules
Retrieves a list of all saved schedule names.

- **URL:** `/api/schedule`
- **Method:** `GET`
- **Success Response:**
  - **Code:** 200 OK
  - **Content:** `["Schedule1", "Schedule2"]`

### Get Schedule Details
Retrieves detailed information for a specific schedule.

- **URL:** `/api/schedule?name=<name>`
- **Method:** `GET`
- **Success Response:**
  - **Code:** 200 OK
  - **Content (DoW type):**
    ```json
    {
      "name": "Morning",
      "pump": 1,
      "dow": 127,
      "h": 8,
      "m": 0,
      "duration": [5000, 5000, 5000],
      "type": 0
    }
    ```
  - **Content (Sunrise/Sunset type):**
    ```json
    {
      "name": "Evening",
      "pump": 2,
      "dow": 64,
      "offset": -15,
      "duration": [3000, 3000, 3000],
      "type": 1
    }
    ```

### Add/Update Schedule
Creates a new schedule or updates an existing one using query parameters.

- **URL:** `/api/schedule`
- **Method:** `POST`
- **Query Parameters:**
  - `name`: string (max 15 chars)
  - `type`: `dow`, `sunrise`, or `sunset`
  - `pump`: bitmask (1: Pump 1, 2: Pump 2, 3: Both)
  - `dow`: bitmask (1: Sun, 2: Mon, 4: Tue, 8: Wed, 16: Thu, 32: Fri, 64: Sat)
  - `hour`: 0-23 (required for `type=dow`)
  - `min`: 0-59 (required for `type=dow`)
  - `off`: signed integer minutes (required for `type=sunrise/sunset`)
  - `durd`: Dry duration (ms)
  - `durm`: Moderate duration (ms)
  - `durw`: Wet duration (ms)
- **Success Response:**
  - **Code:** 202 Accepted
  - **Content:** `OK`

### Delete Schedule
Deletes a schedule by its name.

- **URL:** `/api/schedule?name=<name>`
- **Method:** `DELETE`
- **Success Response:**
  - **Code:** 202 Accepted
  - **Content:** `OK`

---

## System Configuration

### Set WiFi Settings
Updates the station credentials. Note: Device may reconnect after this.

- **URL:** `/api/wifi`
- **Method:** `POST`
- **Query Parameters:**
  - `ssid`: Network name
  - `pwd`: Network password
- **Success Response:**
  - **Code:** 202 Accepted
  - **Content:** `OK`

### Set System Time
Synchronizes the internal system clock using a Unix timestamp.

- **URL:** `/api/time`
- **Method:** `POST`
- **Request Body:**
  ```json
  { "now": 1735293600 }
  ```
- **Success Response:**
  - **Code:** 202 Accepted
  - **Content:** `OK`

### Get Firmware Information
Returns metadata about the running firmware and SDK.

- **URL:** `/api/fwinfo`
- **Method:** `GET`
- **Success Response:**
  - **Code:** 200 OK
  - **Content:**
    ```json
    {
      "sdk": "v5.x",
      "fw": "1.0.0",
      "compDate": "Dec 27 2025"
    }
    ```

---

## Web Interface

### Index Page
Serves the built-in management web interface.

- **URL:** `/`
- **Method:** `GET`
- **Success Response:**
  - **Code:** 200 OK
  - **Content:** HTML
