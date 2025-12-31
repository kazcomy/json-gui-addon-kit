# Context Overview

The system is a SPI-controlled display slave that renders UI on an SSD1306 panel.
A host MCU provisions UI structure; local buttons on the slave drive UI state changes.
When local input updates the UI, the slave notifies the host via an interrupt line,
and the host fetches UI state over SPI.

```mermaid
flowchart LR
    User
    Converter["JSON Converter"]
    Host["Host MCU<br/>SPI Master"]
    Slave["gfx_slave<br/>CH32V003"]
    Panel["SSD1306<br/>I2C Display"]
    User -->|JSON| Converter
    Converter -->|Shrinked JSON| Host
    Host -->|SPI | Slave
    Slave -->|INT| Host
    Slave -->|I2C| Panel
```
## External actors
- Host MCU: provisions UI, receives interrupts from the slave, and queries UI state.
- SSD1306 panel: display target (128x32 or 128x64).

# Host Workflow Summary
In order to reduce slave's memory consumption, host shall convert json to predefined format by converter beforehand.

```mermaid
sequenceDiagram
    participant Host as Host MCU (SPI Master)
    participant Slave as gfx_slave
    participant User as User

    Host->>Host: Initialize SPI 
    Host->>Slave: PING 
    Slave-->>Host: RC + version + caps
    Host->>Slave: Read status
    Slave-->>Host: RC + status

    loop Provision elements 
        Host->>Slave: JSON elements for each line
        Slave->>Slave: Parse and Store objects
        Slave-->>Host: RC
    end

    loop Runtime updates
        Host->>Slave: JSON update 
        Slave-->>Host: RC
    end

    loop Local input and host sync
        User->>Slave: Button release
        Slave->>Slave: Update UI state
        Slave-->>Host: INT (state changed)
        Host->>Slave: Read status / element state
        Slave-->>Host: RC + state
    end
```
