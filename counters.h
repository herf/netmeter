#ifndef __PDH_H__
#define __PDH_H__

#include <windows.h>
#include <stdio.h>
#pragma hdrstop
#include "pdh.h"

enum {kHistory = 1024};

// ================================================================================================
// Single-query PDH wrapper with history
// ================================================================================================
class PDHQuery {

	friend class PDH;

	HQUERY query;
	HCOUNTER counter;
	TCHAR counterpath[256];
	long history[kHistory];
	long historysize;

public:
	PDHQuery() : query(NULL), counter(NULL), historysize(0) {
		PdhOpenQuery(NULL, 0, &query);
	}
	~PDHQuery() {
		PdhCloseQuery(query);
	}
	
	void SetPath(char *path);
	int GetData();
};

// ================================================================================================
// Class to manage multiple queries and add more
// ================================================================================================
class PDH {

	struct PDHList {
		PDHList() { next = NULL; }

		PDHQuery q;
		PDHList *next;
	};

	int counters;
	PDHList *head;

public:
	PDH() : head(NULL), counters(0) {}
	
	long Sum(int timeago);
	void AddCounter(char *path);
	void ChooseUI();
	void AddObjects(char *objsearch, char *countersearch = NULL, char *ifsearch = NULL);

	void Tick();
};

#endif // __PDH_H__
