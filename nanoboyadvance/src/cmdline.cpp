/*
* Copyright (C) 2015 Frederic Meyer
*
* This file is part of nanoboyadvance.
*
* nanoboyadvance is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 2 of the License, or
* (at your option) any later version.
*
* nanoboyadvance is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with nanoboyadvance. If not, see <http://www.gnu.org/licenses/>.
*/

#include <cstring>
#include <iostream>
#include "cmdline.h"

using namespace std;

// Called when none or invalid arguments are passed
void usage()
{
    cout << "Usage: ./nanoboyadvance [--debug] [--strict] [--bios bios_file] rom_file" << endl;
}

// Takes commandline parameters and parses them
CmdLine* parse_parameters(int argc, char** argv)
{
    CmdLine* cmdline = (CmdLine*)malloc(sizeof(CmdLine));
    int current_argument = 1;

    // Init cmdline
    cmdline->bios_file = (char*)"bios.bin";
    cmdline->use_bios = false;
    cmdline->debug = false;
    cmdline->strict = false;

    // Process arguments
    while (current_argument < argc)
    {
        bool no_switch = false;

        // Handle optional parameters
        if (strcmp(argv[current_argument], "--bios") == 0)
        {
            cmdline->use_bios = true;
            if (argc > current_argument + 1)
            {
                cmdline->bios_file = argv[++current_argument];
            }
            else
            {
                usage();
                return 0;
            }
        }
        else if (strcmp(argv[current_argument], "--debug") == 0)
        {
            cmdline->debug = true;
        }
        else if (strcmp(argv[current_argument], "--strict") == 0)
        {
            cmdline->strict = true;
        }
        else
        {
            no_switch = true;
        }

        // Get rom path
        if (current_argument == argc - 1)
        {
            if (!no_switch)
            {
                usage();
                return 0;
            }
            cmdline->rom_file = argv[current_argument];
        }

        // Update argument counter
        current_argument++;
    }
    return cmdline;
}
