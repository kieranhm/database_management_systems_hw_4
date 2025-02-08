//Name: Yu-Siang Chou
//OSU email: chouyu@oregonstate.edu
//ONID: chouyu

#include <string>
#include <iostream>
#include <stdexcept>
#include "classes.h"

using namespace std;


int main(int argc, char* const argv[]) {

    // Create the index
    LinearHashIndex hashIndex("EmployeeIndex.dat");
    hashIndex.createFromFile("Employee.csv");


    // TODO: You'll receive employee IDs as arguments, process them to retrieve the record, or display a message if not found. 
    for (int i = 1; i < argc; ++i) {
        int employeeID = stoi(argv[i]);
        cout << "Searching for Employee ID: " << employeeID << "...\n";
        hashIndex.findAndPrintEmployee(employeeID);
    }

    return 0;
}

