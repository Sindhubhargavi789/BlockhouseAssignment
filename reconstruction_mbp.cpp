// =========================
// reconstruction_mbp10.cpp
// =========================
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <unordered_map>
#include <vector>
#include <string>
#include <iomanip>
#include <algorithm>

struct Order {
    long order_id;
    char side; // 'B' or 'A'
    double price;
    int size;
};

struct MBORecord {
    std::string ts_event;
    std::string action;
    char side;
    double price;
    int size;
    long order_id;
};

class OrderBook {
private:
    std::map<double, std::vector<Order>, std::greater<>> bids;  // descending order for bids
    std::map<double, std::vector<Order>> asks;  // ascending order for asks
    std::unordered_map<long, Order> order_map;

public:
    void add_order(const Order& order) {
        if (order.side == 'B') {
            bids[order.price].push_back(order);
        } else {
            asks[order.price].push_back(order);
        }
        order_map[order.order_id] = order;
    }

    void modify_order(long order_id, int new_size) {
        if (order_map.find(order_id) == order_map.end()) return;
        
        Order& ord = order_map[order_id];
        ord.size = new_size;
        
        if (ord.side == 'B') {
            auto& orders = bids[ord.price];
            for (auto& o : orders) {
                if (o.order_id == order_id) {
                    o.size = new_size;
                    break;
                }
            }
        } else {
            auto& orders = asks[ord.price];
            for (auto& o : orders) {
                if (o.order_id == order_id) {
                    o.size = new_size;
                    break;
                }
            }
        }
    }

    void cancel_order(long order_id) {
        if (order_map.find(order_id) == order_map.end()) return;
        
        Order ord = order_map[order_id];
        if (ord.side == 'B') {
            auto& orders = bids[ord.price];
            orders.erase(std::remove_if(orders.begin(), orders.end(),
                [order_id](const Order& o) { return o.order_id == order_id; }), orders.end());
            if (orders.empty()) {
                bids.erase(ord.price);
            }
        } else {
            auto& orders = asks[ord.price];
            orders.erase(std::remove_if(orders.begin(), orders.end(),
                [order_id](const Order& o) { return o.order_id == order_id; }), orders.end());
            if (orders.empty()) {
                asks.erase(ord.price);
            }
        }
        order_map.erase(order_id);
    }

    void apply_trade(double price, int size, char original_side) {
        // Trade affects the opposite side of the book
        // If trade side is 'A', it removes from bid side
        // If trade side is 'B', it removes from ask side
        if (original_side == 'A') {
            // Trade on ask side means bid orders are consumed
            auto it = bids.find(price);
            if (it == bids.end()) return;
            
            int remaining = size;
            auto& orders = it->second;
            
            for (auto oit = orders.begin(); oit != orders.end() && remaining > 0; ) {
                if (oit->size <= remaining) {
                    remaining -= oit->size;
                    order_map.erase(oit->order_id);
                    oit = orders.erase(oit);
                } else {
                    oit->size -= remaining;
                    order_map[oit->order_id].size = oit->size;
                    remaining = 0;
                }
            }
            
            if (orders.empty()) {
                bids.erase(price);
            }
        } else if (original_side == 'B') {
            // Trade on bid side means ask orders are consumed
            auto it = asks.find(price);
            if (it == asks.end()) return;
            
            int remaining = size;
            auto& orders = it->second;
            
            for (auto oit = orders.begin(); oit != orders.end() && remaining > 0; ) {
                if (oit->size <= remaining) {
                    remaining -= oit->size;
                    order_map.erase(oit->order_id);
                    oit = orders.erase(oit);
                } else {
                    oit->size -= remaining;
                    order_map[oit->order_id].size = oit->size;
                    remaining = 0;
                }
            }
            
            if (orders.empty()) {
                asks.erase(price);
            }
        }
    }

    void get_top10_levels(std::vector<std::tuple<double, int, int>>& bid_levels,
                         std::vector<std::tuple<double, int, int>>& ask_levels) {
        bid_levels.clear();
        ask_levels.clear();
        
        // Get top 10 bid levels (highest prices first)
        for (const auto& entry : bids) {
            double price = entry.first;
            const auto& orders = entry.second;
            int total_size = 0;
            int count = orders.size();
            
            for (const auto& order : orders) {
                total_size += order.size;
            }
            
            bid_levels.emplace_back(price, total_size, count);
            if (bid_levels.size() == 10) break;
        }
        
        // Get top 10 ask levels (lowest prices first)
        for (const auto& entry : asks) {
            double price = entry.first;
            const auto& orders = entry.second;
            int total_size = 0;
            int count = orders.size();
            
            for (const auto& order : orders) {
                total_size += order.size;
            }
            
            ask_levels.emplace_back(price, total_size, count);
            if (ask_levels.size() == 10) break;
        }
    }

    void clear() {
        bids.clear();
        asks.clear();
        order_map.clear();
    }
};

std::vector<std::string> split_csv(const std::string& line) {
    std::vector<std::string> tokens;
    std::stringstream ss(line);
    std::string token;
    
    while (std::getline(ss, token, ',')) {
        tokens.push_back(token);
    }
    return tokens;
}

MBORecord parse_mbo_record(const std::vector<std::string>& fields) {
    MBORecord record;
    record.ts_event = fields[1];
    record.action = fields[5];
    record.side = fields[6].empty() ? 'N' : fields[6][0];
    record.price = fields[7].empty() ? 0.0 : std::stod(fields[7]);
    record.size = fields[8].empty() ? 0 : std::stoi(fields[8]);
    record.order_id = fields[10].empty() ? 0 : std::stol(fields[10]);
    return record;
}

void write_mbp_snapshot(std::ofstream& out, const std::string& timestamp,
                       const std::vector<std::tuple<double, int, int>>& bids,
                       const std::vector<std::tuple<double, int, int>>& asks,
                       const std::vector<std::string>& fields) {
    out << timestamp;

    // Output the extra columns (use empty string if missing)
    out << "," << (fields.size() > 0 ? fields[0] : "");  // ts_recv
    out << "," << (fields.size() > 2 ? fields[2] : "");  // r_type
    out << "," << (fields.size() > 3 ? fields[3] : "");  // publisher
    out << "," << (fields.size() > 4 ? fields[4] : "");  // instrument_id
    out << "," << (fields.size() > 5 ? fields[5] : "");  // action
    out << "," << (fields.size() > 6 ? fields[6] : "");  // side
    out << "," << (fields.size() > 9 ? fields[9] : "");  // depth (channel_id)
    out << "," << (fields.size() > 7 ? fields[7] : "");  // price
    out << "," << (fields.size() > 8 ? fields[8] : "");  // size
    out << "," << (fields.size() > 11 ? fields[11] : ""); // flags
    out << "," << (fields.size() > 12 ? fields[12] : ""); // ts_in_delta
    out << "," << (fields.size() > 13 ? fields[13] : ""); // sequence
    out << "," << (fields.size() > 14 ? fields[14] : ""); // symbol
    out << "," << (fields.size() > 10 ? fields[10] : ""); // order_id

    // Write bid and ask levels interleaved
    for (int i = 0; i < 10; ++i) {
        // Bid level
        if (i < bids.size()) {
            out << "," << std::get<0>(bids[i]) << "," << std::get<1>(bids[i]) << "," << std::get<2>(bids[i]);
        } else {
            out << ",,,";
        }
        // Ask level
        if (i < asks.size()) {
            out << "," << std::get<0>(asks[i]) << "," << std::get<1>(asks[i]) << "," << std::get<2>(asks[i]);
        } else {
            out << ",,,";
        }
    }
    out << "\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: ./reconstruction_mbp10 mbo.csv\n";
        return 1;
    }

    std::ifstream infile(argv[1]);
    if (!infile.is_open()) {
        std::cerr << "Error: Cannot open input file " << argv[1] << "\n";
        return 1;
    }

    std::ofstream outfile("mbp_output.csv");
    if (!outfile.is_open()) {
        std::cerr << "Error: Cannot create output file\n";
        return 1;
    }

    std::string line;
    std::getline(infile, line); // Skip header

    OrderBook book;
    std::vector<MBORecord> pending_records;
    bool first_row = true;

    // Write header
    outfile << "ts_event";
    // Add new columns to header
    outfile << ",ts_recv,r_type,publisher,instrument_id,action,side,depth,price,size,flags,ts_in_delta,sequence,symbol,order_id";
    for (int i = 0; i < 10; ++i) {
        outfile << ",bid_px_" << std::setw(2) << std::setfill('0') << i
                << ",bid_sz_" << std::setw(2) << std::setfill('0') << i
                << ",bid_ct_" << std::setw(2) << std::setfill('0') << i
                << ",ask_px_" << std::setw(2) << std::setfill('0') << i
                << ",ask_sz_" << std::setw(2) << std::setfill('0') << i
                << ",ask_ct_" << std::setw(2) << std::setfill('0') << i;
    }
    outfile << "\n";

    while (std::getline(infile, line)) {
        auto fields = split_csv(line);
        if (fields.size() < 11) continue;
        
        MBORecord record = parse_mbo_record(fields);
        
        // Skip first clear record
        if (first_row && record.action == "R") {
            first_row = false;
            continue;
        }
        first_row = false;

        // Handle T->F->C sequence
        if (record.action == "T") {
            pending_records.clear();
            pending_records.push_back(record);
            continue;
        } else if (record.action == "F" && !pending_records.empty() && 
                   pending_records.back().action == "T") {
            pending_records.push_back(record);
            continue;
        } else if (record.action == "C" && pending_records.size() >= 2 && 
                   pending_records[0].action == "T" && pending_records[1].action == "F") {
            // Process T->F->C sequence
            MBORecord& trade_record = pending_records[0];
            
            // Only process if trade side is not 'N'
            if (trade_record.side != 'N') {
                // Apply trade to opposite side of the book
                book.apply_trade(trade_record.price, trade_record.size, trade_record.side);
            }
            
            pending_records.clear();
        } else {
            // Clear any pending records if we don't get the expected sequence
            pending_records.clear();
            
            // Process regular actions
            Order order;
            order.order_id = record.order_id;
            order.side = record.side;
            order.price = record.price;
            order.size = record.size;

            if (record.action == "A") {
                book.add_order(order);
            } else if (record.action == "M") {
                book.modify_order(order.order_id, order.size);
            } else if (record.action == "C") {
                book.cancel_order(order.order_id);
            }
        }

        // Generate snapshot after each event
        std::vector<std::tuple<double, int, int>> bid_levels, ask_levels;
        book.get_top10_levels(bid_levels, ask_levels);
        write_mbp_snapshot(outfile, record.ts_event, bid_levels, ask_levels, fields);
    }

    infile.close();
    outfile.close();
    return 0;
}