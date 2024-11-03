## Sequince diagram

```mermaid
sequenceDiagram
    actor Writer
    actor Reader
    participant SMT as "SharedMemoryTransport"
    participant SMS as "Shared Memory Structure"

    %% Initialization
    Writer ->> SMT: instantiate(name, bufferSize)
    SMT ->> SMS: access/create("SharedMemoryStructure")
    SMS ->> SMT: return rawPointer

    Reader ->> SMT: instantiate(name, bufferSize)
    SMT ->> SMS: access "SharedMemoryStructure"

    %% Write Data
    Writer ->> SMT: sendData(buffer)
    SMT ->> SMS: acquire lock
    SMS ->> SMS: wait until !dataReady
    SMT ->> SMS: memcpy(data, buffer)
    SMT ->> SMS: set dataReady true
    SMT ->> SMS: notify_all
    SMT ->> SMS: release lock

    %% Read Data
    Reader ->> SMT: receiveData()
    SMT ->> SMS: acquire lock
    SMS ->> SMS: wait until dataReady or finished
    SMT ->> SMS: memcpy(localBuffer, data)
    SMT ->> SMS: set dataReady false
    SMT ->> SMS: notify_all
    SMT ->> Reader: return localBuffer
    SMT ->> SMS: release lock

    %% Finish Communication
    Process ->> SMT: finish()
    SMT ->> SMS: acquire lock
    SMT ->> SMS: setFinished(true)
    SMT ->> SMS: notify_all
    SMT ->> SMS: release lock

    %% Cleanup
    Process ->> SMT: destructor
    SMT ->> SMS: acquire lock
    SMS ->> SMS: check if active_process_count == 0
    SMT ->> SMS: destroy SharedMemoryStructure
```