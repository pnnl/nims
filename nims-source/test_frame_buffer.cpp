/*
 *  Nekton Interaction Monitoring System (NIMS)
 *
 *  test_frame_buffer.cpp
 *  
 *  Created by Shari Matzer on 04/20/2015.
 *  Copyright 2015 Pacific Northwest National Laboratory. All rights reserved.
 *
 */
#include <iostream> // cout, cin, cerr
//#include <string>   // for strings
#include <thread>
#include <chrono>
#include <unistd.h>


using namespace std;

int main (int argc, char * const argv[]) {
    
	//--------------------------------------------------------------------------
	// DO STUFF
	cout << endl << "Starting " << argv[0] << endl;

    pid_t pidput = fork();
    if (0 == pidput) // first child
    {
        
        char *newargv[] = { "./test_frame_buffer_put", (char *)NULL };
        char *newenv[] = { (char *)NULL };
        execve(newargv[0], newargv, newenv);
        perror("exec put");
        
    }
    else if (-1 == pidput) // error
    {
        perror("fork put");
        
        
    }
    else  // parent process
    {
        pid_t pidget = fork();
        if (0 == pidget) // second child
        {
            std::this_thread::sleep_for(std::chrono::seconds(3));
            char *newargv[] = { "./test_frame_buffer_get", (char *)NULL };
            char *newenv[] = { (char *)NULL };
            execve(newargv[0], newargv, newenv);
            perror("exec get"); // exec only returns on error
        }
        else if (-1 == pidget) // error
        {
            perror("fork get");
        }
        else // still parent
        {
        }
    }

	
	   
	cout << endl << "Ending " << argv[0] << endl << endl;
    return 0;
}
