#include "pin.H"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <map>
#include <vector>
#include <algorithm>

using std::cerr;
using std::endl;
using std::ofstream;
using std::string;
using std::map;
using std::vector;
using std::pair;

struct BBLEntry {
    UINT64 execCount = 0;
    UINT64 takenCount = 0;
    UINT64 fallthroughCount = 0;
    bool isConditionalJump = false;
    map<ADDRINT, UINT64> indirectTargets;
};

map<ADDRINT, BBLEntry> bblEntries;

VOID count_bbl_exec(ADDRINT addr) {
    bblEntries[addr].execCount++;
}

VOID count_taken(ADDRINT addr) {
    bblEntries[addr].takenCount++;
}

VOID count_fallthrough(ADDRINT addr) {
    bblEntries[addr].fallthroughCount++;
}

VOID count_branch_indirect(ADDRINT src, ADDRINT target, BOOL taken) {
    if (taken) {
        bblEntries[src].indirectTargets[target]++;
    }
}

VOID Trace(TRACE trace, VOID *v) {
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
        ADDRINT bblAddr = BBL_Address(bbl);
        
        BBL_InsertCall(bbl, IPOINT_ANYWHERE, (AFUNPTR)count_bbl_exec, IARG_ADDRINT, bblAddr, IARG_END);

        INS tail = BBL_InsTail(bbl);

        if (INS_IsBranch(tail) && INS_HasFallThrough(tail)) {
            bblEntries[bblAddr].isConditionalJump = true;
            INS_InsertCall(tail, IPOINT_TAKEN_BRANCH, (AFUNPTR)count_taken, IARG_ADDRINT, bblAddr, IARG_END);
            INS_InsertCall(tail, IPOINT_AFTER, (AFUNPTR)count_fallthrough, IARG_ADDRINT, bblAddr, IARG_END);
        } else if (INS_IsBranch(tail) && INS_IsIndirectControlFlow(tail)) {
            INS_InsertCall(tail, IPOINT_BEFORE, (AFUNPTR)count_branch_indirect, IARG_ADDRINT, bblAddr,
                           IARG_BRANCH_TARGET_ADDR, IARG_BRANCH_TAKEN, IARG_END);
        }
    }
}

VOID Fini(INT32 code, VOID *v) {
    std::ofstream out("edge-profile.csv");

    vector<pair<ADDRINT, BBLEntry>> sorted;
    for (auto &entry : bblEntries) {
        sorted.push_back(entry);
    }

    std::sort(sorted.begin(), sorted.end(),
              [](const pair<ADDRINT, BBLEntry> &a, const pair<ADDRINT, BBLEntry> &b) {
                  return a.second.execCount > b.second.execCount;
              });

    for (const auto &entry : sorted) {
        const ADDRINT addr = entry.first;
        const BBLEntry &bbl = entry.second;

        out << std::hex << addr << ", " << std::dec << bbl.execCount << ", "
            << bbl.takenCount << ", ";

        if (bbl.isConditionalJump) {
            out << bbl.fallthroughCount;
        } else {
            out << "0";
        }

        // Top 10 indirect targets
        vector<pair<ADDRINT, UINT64>> targets(bbl.indirectTargets.begin(), bbl.indirectTargets.end());
        std::sort(targets.begin(), targets.end(),
                  [](const pair<ADDRINT, UINT64> &a, const pair<ADDRINT, UINT64> &b) {
                      return a.second > b.second;
                  });

        int count = 0;
        for (auto &t : targets) {
            if (count++ >= 10) break;
            out << ", <" << std::hex << t.first << ", " << std::dec << t.second << ">";
        }

        out << endl;
    }

    out.close();
}

int main(int argc, char *argv[]) {
    if (PIN_Init(argc, argv)) {
        cerr << "PIN init failed.\n";
        return 1;
    }

    TRACE_AddInstrumentFunction(Trace, 0);
    PIN_AddFiniFunction(Fini, 0);

    // does not return
    PIN_StartProgram();
    return 0;
}
