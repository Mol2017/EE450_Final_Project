# EE450_Final_Project

The project is the c implementation of a simplified version of a blockchain service called "Alichain", where clients issue a request for finding their current amount of coins in their account, transfer them to another client and provide a file statement with all the transactions in order.


## Compile & Run

```bash
# Compile
make clean
make all
# Boot up servers
./serverM
./serverA
./serverB
./serverC
# Run clients
./clientA <username1>
./clientA <username1> <username2> <transfer amount>
./clientA TXLIST
./clientB <username1>
./clientB <username1> <username2> <transfer amount>
./clientB TXLIST
```

## What I've done

- [x] Phase 1: Establish connection between clients and main Server
- [x] Phase 2A: Main Server connects to backend servers and proceeds to retrieve the information.
- [x] Phase 2B: Main Server computes the operation and replies to the client.
- [x] Phase 3: Main Server provides a file statement with all the transactions in order.

## Code File Explanation

```bash
1) clientA.c
    (check balance, transfer coins and provide transaction flow)
    - Establish TCP connection with main server
    - Send requests to main server (TCP)
    - Receive feedbacks from main server (TCP)

2) clientB.c
    (check balance, transfer coins and provide transaction flow)
    - Establish TCP connection with main server
    - Send requests to main server (TCP)
    - Receive feedbacks from main server (TCP)

3) serverM.c
    (contact backend servers to generate feedbacks for client requests)
    - Establish TCP connection with clients
    - Set up UDP socket
    - Receive requests from clients (TCP)
    - Contact backend servers and retrieve information (UDP)
    - Process information and generate feedbacks for clients
    - Forward feedbacks to clients (TCP)
    - Generate transaction flow into "alichain.txt"

4) serverA.cpp
    (provide transaction records to main server, write records to database)
    - Create UDP socket
    - Receive requests from main server (UDP)
    - Extract appropriate records in database and forward to main server (UDP)
    - Write transaction records into database "block1.txt"


5) serverB.cpp
    (provide transaction records to main server, write records to database)
    - Create UDP socket
    - Receive requests from main server (UDP)
    - Extract appropriate records in database and forward to main server (UDP)
    - Write transaction records into database "block2.txt"

6) serverC.cpp
    (provide transaction records to main server, write records to database)
    - Create UDP socket
    - Receive requests from main server (UDP)
    - Extract appropriate records in database and forward to main server (UDP)
    - Write transaction records into database "block3.txt"
```

## Exchanged Message Format

```bash
1) client A/B:
    Request messages sent to main server:
    - CHECK WALLET  ->  '0'   <username>
    - TXCOINS       ->  '1'   <username1>   <username2>   <transfer amount> 
    - TXLIST        ->  '3'

2) main server
    Request messages sent to backend servers:
    - CHECK WALLET  ->  '0'   <username>
    - TXCOINS       ->  '1'   <username1>   <username2>
    - WRITE_LOGS    ->  '2'
    - TXLIST        ->  '3'
    Feedback messages sent to clients:
    - CHECK WALLET (failure)    ->  '0'
    - CHECK WALLET (success)    ->  '1'   <user balance>
    - TXCOINS (success)         ->  '0'   <sender balance after tx>  
    - TXCOINS (insuff fund)     ->  '1'   <sender balance>
    - TXCOINS (sender absent)   ->  '2'
    - TXCOINS (receiver absent) ->  '3'
    - TXCOINS (both absent)     ->  '4'
    - TXLIST (success)          ->  '1'

3) server A/B/C
    Feedback messages sent to main server:
    - CHECK WALLET          ->  <transaction records>
    - TXCOINS               ->  <transaction records> + <max transaction id>
    - WRITE_LOGS (success)  ->  '1'
    - TXLIST                ->  <transaction records>
```

## Idiosyncrasy

    Program would function erroneously if:
    - One line size in block1/2/3.txt exceeds 50 bytes
    - Exchanged messages exceed 10240 bytes

## Reused Code

    1) Refer to Beej's Code (http://www.beej.us/guide/bgnet/)
        - Socket creation
        - TCP connection & data transfer
        - UDP data transfer

