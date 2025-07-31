# README.txt

## Market By Price (MBP) Order Book Reconstruction

This application reconstructs a Market By Price (MBP) order book from a Market By Order (MBO) CSV file and outputs a CSV snapshot of the top 10 bid/ask levels after each event, along with all original event fields.

---

## Optimization Steps Taken

1. **Efficient Data Structures**
   - Uses `std::map<double, std::vector<Order>, std::greater<>>` for bids (descending) and `std::map<double, std::vector<Order>>` for asks (ascending) to allow fast access to price levels and efficient top-10 extraction.
   - Uses `std::unordered_map<long, Order>` for O(1) lookup, modification, and cancellation of orders by order ID.

2. **Avoided Structured Bindings**
   - All code uses `.first` and `.second` for map iteration to maximize compatibility with all C++17 compilers, including those with incomplete support for structured bindings.

3. **Minimal Data Copying**
   - Order objects are only copied when necessary. Most operations use references to avoid unnecessary copying.

4. **Snapshot Generation**
   - Only the top 10 levels are extracted and output after each event, minimizing memory and computation.

5. **Robust CSV Parsing**
   - Handles missing or empty fields gracefully, ensuring the application does not crash on malformed input.

6. **Special Event Handling**
   - Implements logic for T->F->C event sequences, which are common in MBO feeds, ensuring correct trade application.

---

## Special Notes for Running the Code

- **Compiler Requirements:**  
  Requires a C++17-compliant compiler (e.g., g++ 7.1+).  
  On Windows, use a recent MinGW-w64 or MSVC toolchain.

- **Input File:**  
  The input CSV file (e.g., `mbo.csv`) must have a header row and follow the expected column order.

- **Output File:**  
  The output is always written to `mbp_output.csv` in the current directory.

- **Command Line Usage:**  
  ```
  ./reconstruction_mbp mbo.csv
  ```

- **Column Order in Output:**  
  The output CSV contains:
  - `ts_event` (event timestamp)
  - All original MBO columns: `ts_recv`, `r_type`, `publisher`, `instrument_id`, `action`, `side`, `depth`, `price`, `size`, `flags`, `ts_in_delta`, `sequence`, `symbol`, `order_id`
  - 10 bid and 10 ask levels: each with price, size, and count columns

- **Performance:**  
  The application is optimized for speed and memory usage, and can process large MBO files efficiently.

- **Error Handling:**  
  If the input file cannot be opened, or the output file cannot be created, the program will print an error and exit.

- **Extensibility:**  
  The code is modular and can be extended for more advanced analytics or different output formats.

---



- **Extensibility:**  
  The code is modular and can be extended for more advanced analytics or different output formats.

---
