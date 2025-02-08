#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <cstring>
#include <cmath>

using namespace std;

class Record {
public:
    int id, manager_id; // Employee ID and their manager's ID
    string bio, name; // Fixed length string to store employee name and biography

    Record(vector<string> &fields) {
        id = stoi(fields[0]);
        name = fields[1];
        bio = fields[2];
        manager_id = stoi(fields[3]);
    }

    // Function to get the size of the record
    int get_size() {
        // sizeof(int) is for storing name/bio size in serialize function
        return sizeof(id) + sizeof(manager_id) + sizeof(int) + name.size() + sizeof(int) + bio.size(); 
    }

    // Function to serialize the record for writing to file
    string serialize() const {
        ostringstream oss;
        oss.write(reinterpret_cast<const char *>(&id), sizeof(id));
        oss.write(reinterpret_cast<const char *>(&manager_id), sizeof(manager_id));
        int name_len = name.size();
        int bio_len = bio.size();
        oss.write(reinterpret_cast<const char *>(&name_len), sizeof(name_len));
        oss.write(name.c_str(), name.size());
        oss.write(reinterpret_cast<const char *>(&bio_len), sizeof(bio_len));
        oss.write(bio.c_str(), bio.size());
        return oss.str();
    }

    void print() {
        cout << "\tID: " << id << "\n";
        cout << "\tNAME: " << name << "\n";
        cout << "\tBIO: " << bio << "\n";
        cout << "\tMANAGER_ID: " << manager_id << "\n";
    }
};

class Page {
public:
    vector<Record> records; // Data_Area containing the records
    vector<pair<int, int>> slot_directory; // Slot directory containing offset and size of each record
    int cur_size = sizeof(int); // Current size of the page including the overflow page pointer. if you also write the length of slot directory change it accordingly.
    int overflowPointerIndex;  // Initially set to -1, indicating the page has no overflow page. 
                               // Update it to the position of the overflow page when one is created.



    // Constructor
    Page() : overflowPointerIndex(-1) {}

    // Function to insert a record into the page
    bool insert_record_into_page(Record r) {
        int record_size = r.get_size();
        int slot_size = sizeof(int) * 2;
        if (cur_size + record_size + slot_size > 4096) { //Check if page size limit exceeded, considering slot directory size
            return false; // Cannot insert the record into this page
        } else {
            records.push_back(r);
            cur_size += record_size + slot_size; //Updated
             // TO_DO: update slot directory information
            slot_directory.push_back({cur_size, record_size});
            return true;
        }
    }

    // Function to write the page to a binary output stream. 
    void write_into_data_file(ostream &out) const {
        char page_data[4096] = {0}; // Buffer to hold page data
        int offset = 0;

        // Write records into page_data buffer
        for (const auto &record: records) {
            string serialized = record.serialize();
            memcpy(page_data + offset, serialized.c_str(), serialized.size());
            offset += serialized.size();
        }

        // TODO:
        //  - Write slot_directory in reverse order into page_data buffer.
        //  - Write overflowPointerIndex into page_data buffer.
        //  You should write the first entry of the slot_directory, which have the info about the first record at the bottom of the page, before overflowPointerIndex.
        for (const auto &entry : slot_directory) {
            memcpy(page_data + offset, &entry.first, sizeof(entry.first));
            offset += sizeof(entry.first);
            memcpy(page_data + offset, &entry.second, sizeof(entry.second));
            offset += sizeof(entry.second);
        }
        memcpy(page_data + offset, &overflowPointerIndex, sizeof(overflowPointerIndex));
        // Write the page_data buffer to the output stream
        out.write(page_data, sizeof(page_data));
    }

    // Function to read a page from a binary input stream
    bool read_from_data_file(istream &in) {
        char page_data[4096] = {0}; // Buffer to hold page data
        in.read(page_data, 4096); // Read data from input stream

        streamsize bytes_read = in.gcount();
        if (bytes_read == 4096) {
            // TODO: Process data to fill the records, slot_directory, and overflowPointerIndex
            int slot_directory_start = 4096 - sizeof(int) - sizeof(int) * 2; // Overflow pointer is at the last int in the page
            while (slot_directory_start >= 0) {
                int offset, size;
                memcpy(&offset, page_data + slot_directory_start, sizeof(int));
                memcpy(&size, page_data + slot_directory_start + sizeof(int), sizeof(int));
                slot_directory.push_back({offset, size});
                slot_directory_start -= sizeof(int) * 2; // Move to the next slot entry
            }
            memcpy(&overflowPointerIndex, page_data + 4096 - sizeof(int), sizeof(int));
            for (const auto& slot : slot_directory) {
                int record_offset = slot.first;
                int record_size = slot.second;

                // Extract the record from the page data
                string record_data(page_data + record_offset, record_size);

                // We need to deserialize the record_data into a Record object
                // (Assuming you have a deserialization function for the Record class)
                istringstream record_stream(record_data);
                string item;
                vector<string> fields;
                while (getline(record_stream, item, ',')) {
                    fields.push_back(item);
                }
                Record r(fields);
                records.push_back(r);
            }
            return true;
        }

        if (bytes_read > 0) {
            cerr << "Incomplete read: Expected 4096 bytes, but only read " << bytes_read << " bytes." << endl;
        }

        return false;
    }
};

class LinearHashIndex {
private:
    const size_t maxCacheSize = 1; // Maximum number of pages in the buffer
    const int Page_SIZE = 4096; // Size of each page in bytes
    int n;  // The number of indexes (pages) being used
    int i;	// The number of least-significant-bits of h(id) to check. Will need to increase i once n > 2^i
    int numRecords;    // Records currently in index. Used to test whether to increase n
    string fileName;

    // Function to compute hash value for a given ID
    int compute_hash_value(int id) {
        int hash_value;

        // TODO: Implement the hash function h = id mod 2^12.
        hash_value = id % (1 << 12);
        return hash_value;
    }

    // Function to add a new record to an existing page in the index file
    void addRecordToIndex(int pageIndex, Page &page, Record &record) {
        // Open index file in binary mode for updating
        fstream indexFile(fileName, ios::binary | ios::in | ios::out);

        if (!indexFile) {
            cerr << "Error: Unable to open index file for adding record." << endl;
            return;
        }

        // TODO: 
        // Add record to the index in the correct block, creating a overflow block if necessary
        int hash_value = compute_hash_value(record.id);

        // 2. Check if there is space in the current page to insert the record
        if (!page.insert_record_into_page(record)) {
            // Page is full, handle overflow
            page.overflowPointerIndex = createOverflowPage(hash_value, record);
        }
        numRecords++;
        // Check and Take neccessary steps if capacity is reached:
        OverflowHandler();
		// increase n; increase i (if necessary); place records in the new bucket that may have been originally misplaced due to a bit flip



        // Seek to the appropriate position in the index file
        indexFile.seekp(pageIndex * Page_SIZE, ios::beg);
        // TODO: Insert record to page and write data to file
        page.write_into_data_file(indexFile);
        // Close the index file
        indexFile.close();
    }

    void OverflowHandler() {
        // TODO:
        // Calculate the average number of records per page
        // Take neccessary steps if capacity is reached
        // increase n; increase i (if necessary); redistribute records accordingly. place records in the new bucket that may have been originally misplaced due to a bit flip.
        float averageRecordsPerPage = static_cast<float>(numRecords) / n;
        if (averageRecordsPerPage > 0.7f) {
            n *= 2;  // Double the number of pages
            i++;     // Increase the number of bits for addressing
            redistributeRecords();
        }
    }

    void redistributeRecords(){
        vector<Record> allRecords;
        // Open the index file in binary mode to read all pages
        fstream indexFile(fileName, ios::binary | ios::in | ios::out);
        for (int pageIndex = 0; pageIndex < n / 2; ++pageIndex) {
            // Seek to the page in the file
            indexFile.seekg(pageIndex * Page_SIZE, ios::beg);

            // Read the page data
            Page page;
            page.read_from_data_file(indexFile);

            // Collect all records from this page
            for (const auto &record : page.records) {
                allRecords.push_back(record);
            }
        }
        // Now redistribute records into the new index structure with the updated number of pages
        for (const auto &record : allRecords) {
            // Compute the new hash value based on the updated number of pages
            int newHashValue = compute_hash_value(record.id) % n;

            // Determine the page index in the new structure
            int newPageIndex = newHashValue;

            // Insert the record into the appropriate page in the new index structure
            // (You may need to implement logic for writing into the new index pages)
            Page newPage;
            bool inserted = newPage.insert_record_into_page(record);
            if (!inserted) {
                // If page is full, create an overflow page and update the overflow pointer
                int overflowPageIndex = createOverflowPage(newHashValue, record);
                // Update the original page's overflow pointer to point to the new overflow page
                // (This step would involve writing to the page's overflowPointerIndex)
            }

            // Write the updated page to the file (This part depends on how the page is updated)
            indexFile.seekp(newPageIndex * Page_SIZE, ios::beg);
            newPage.write_into_data_file(indexFile);
        }

        // Close the index file after redistribution
        indexFile.close();
    }

    int createOverflowPage(int hash_value, Record record){
        Page overflowPage;
        bool inserted = overflowPage.insert_record_into_page(record);
        fstream indexFile(fileName, ios::binary | ios::in | ios::out);
        indexFile.seekp(0, ios::end);
        int overflowPageIndex = static_cast<int>(indexFile.tellp()) / Page_SIZE;

        // Write the overflow page to the file
        overflowPage.write_into_data_file(indexFile);

        // Close the index file
        indexFile.close();

        // Return the index of the new overflow page
        return overflowPageIndex;
    }

    // Function to search for a record by ID in a given page of the index file
    void searchRecordByIdInPage(int pageIndex, int id) {
        // Open index file in binary mode for reading
        ifstream indexFile(fileName, ios::binary | ios::in);

        // Seek to the appropriate position in the index file
        indexFile.seekg(pageIndex * Page_SIZE, ios::beg);

        // Read the page from the index file
        Page page;
        page.read_from_data_file(indexFile);

        // TODO:
        //  - Search for the record by ID in the page
        //  - Check for overflow pages and report if record with given ID is not found
        for (size_t i = 0; i < page.records.size(); ++i) {
            if (page.records[i].id == id) {
                page.records[i].print();  // Print record details
                return;
            }
        }

        if (page.overflowPointerIndex != -1) {
            searchRecordByIdInPage(page.overflowPointerIndex, id);  // Search in overflow page
        }
    }

public:
    LinearHashIndex(string indexFileName) : numRecords(0), fileName(indexFileName) {  
        n = 4; // Start with 4 buckets in index
        i = 2; // Need 2 bits to address 4 buckets
    }

    // Function to create hash index from Employee CSV file
    void createFromFile(string csvFileName) {
        // Read CSV file and add records to index
        // Open the CSV file for reading
        ifstream csvFile(csvFileName);

        string line;
        // Read each line from the CSV file
        while (getline(csvFile, line)) {
            // Parse the line and create a Record object
            stringstream ss(line);
            string item;
            vector<string> fields;
            while (getline(ss, item, ',')) {
                fields.push_back(item);
            }
            Record record(fields);

            // TODO:
            //   - Compute hash value for the record's ID using compute_hash_value() function.
            //   - Insert the record into the appropriate page in the index file using addRecordToIndex() function.
            int hashValue = compute_hash_value(record.id);
            Page page;
            addRecordToIndex(hashValue, page, record);
        }

        // Close the CSV file
        csvFile.close();
    }

    // Function to search for a record by ID in the hash index
    void findAndPrintEmployee(int id) {
        // Open index file in binary mode for reading
        ifstream indexFile(fileName, ios::binary | ios::in);

        // TODO:
        //  - Compute hash value for the given ID using compute_hash_value() function
        //  - Search for the record in the page corresponding to the hash value using searchRecordByIdInPage() function
        int pageIndex = compute_hash_value(id);
        searchRecordByIdInPage(pageIndex, id);

        // Close the index file
        indexFile.close();
    }
};

