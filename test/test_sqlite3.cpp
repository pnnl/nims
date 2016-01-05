// http://programmingknowledgeblog.blogspot.com/2013/05/how-to-install-sqlite-in-ubuntu-and-run.html
// http://stackoverflow.com/questions/10850375/c-create-database-using-sqlite-for-insert-update
//=========================================================
// Name        : SqLite_Test.cpp
// Author      : 
// Version     :
// Copyright   : Your copyright notice
// Description : SqLite_Test.cpp in C++, Ansi-style
//=========================================================

#include <iostream>
#include <sqlite3.h>
using namespace std;


// This is the callback function to display the select data in the table
static int callback(void *NotUsed, int argc, char **argv, char **szColName)
{
  for(int i = 0; i < argc; i++)
  {
    std::cout << szColName[i] << " = " << argv[i] << std::endl;
  }

  std::cout << "\n";

  return 0;
}

int main()
{
  sqlite3 *db;
  char *szErrMsg = 0;

  // open database
  int rc = sqlite3_open("Sqlite_Test.db", &db);

  if(rc)
  {
    std::cout << "Can't open database\n";
  } else {
    std::cout << "Open database successfully\n";
  }

  // prepare our sql statements
  const char *pSQL[6];
  pSQL[0] = "CREATE TABLE Employee(Firstname varchar(30), Lastname varchar(30), Age smallint)";
  pSQL[1] = "INSERT INTO Employee(Firstname, Lastname, Age) VALUES ('Woody', 'Alan', 45)";
  pSQL[2] = "INSERT INTO Employee(Firstname, Lastname, Age) VALUES ('Micheal', 'Bay', 38)";
  pSQL[3] = "SELECT * FROM Employee";

  // execute sql
  for(int i = 3; i < 4; i++)
  {
    rc = sqlite3_exec(db, pSQL[i], callback, 0, &szErrMsg);
    if(rc != SQLITE_OK)
    {
      std::cout << "SQL Error: " << szErrMsg << std::endl;
      sqlite3_free(szErrMsg);
      break;
    }
  }

  // close database
  if(db)
  {
    sqlite3_close(db);
  }

  return 0;
}

