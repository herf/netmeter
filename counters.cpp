#include "counters.h"

void PDHQuery::SetPath(char *path) 
{
	if (counter) {
		PdhRemoveCounter(counter);
	}
	PdhAddCounter(query, path, 0, &counter);
	strcpy(counterpath, path);
}
	
int PDHQuery::GetData() {

	int nonzero = 0;
	PDH_RAW_COUNTER rawcounter;
	PDH_FMT_COUNTERVALUE fmtValue;
	
	if (!counter) return 0;
	
	PdhCollectQueryData(query);
	PdhGetFormattedCounterValue(counter, PDH_FMT_LONG | PDH_FMT_NOSCALE, 0, &fmtValue);
	//PdhGetFormattedCounterValue(counter, PDH_FMT_DOUBLE | PDH_FMT_NOSCALE, 0, &fmtValue);
	PdhGetRawCounterValue(counter, 0, &rawcounter);
	
	historysize ++;
	//history[historysize & 1023] = fmtValue.doubleValue;
	history[historysize % kHistory] = fmtValue.longValue;

	if (fmtValue.longValue != 0) {
	//if (fmtValue.doubleValue != 0) {
		
		char temp[2048];
		sprintf(temp, "%s %8u %I64u\n", counterpath, fmtValue.longValue, (LONG)rawcounter.FirstValue);
		OutputDebugString(temp);

		//printf("%s %8u %I64u\n", counterpath, fmtValue.longValue, (LONG)rawcounter.FirstValue);
		//printf("%s %f %I64u\n", counterpath, fmtValue.doubleValue, (LONG)rawcounter.FirstValue);

		nonzero ++;
	}

	return nonzero;
}

long PDH::Sum(int timeago)
{
	if (timeago > kHistory) return 0;

	long sum = 0;
	for (PDHList *l = head; l; l = l->next) {
		if (timeago > l->q.historysize) return 0;
		sum += l->q.history[(l->q.historysize - timeago) % kHistory];
	}

	return sum;
}

void PDH::AddCounter(char *path) {
	printf("Adding counter %s\n", path);
	PDHList *nl = new PDHList();
	nl->q.SetPath(path);
	nl->next = head;
	head = nl;
}

void PDH::ChooseUI() {
	
	PDH_BROWSE_DLG_CONFIG brwDlgCfg;
	TCHAR szCounterPath[4096];
	
	// Initialize all the fields of a PDH_BROWSE_DLG_CONFIG structure, in
	// preparation for calling PdhBrowseCounters
	
	memset(&brwDlgCfg, 0, sizeof(PDH_BROWSE_DLG_CONFIG));
	
	brwDlgCfg.bSingleCounterPerDialog = 1;
	brwDlgCfg.bLocalCountersOnly = 1;
	
	brwDlgCfg.szReturnPathBuffer = szCounterPath;
	brwDlgCfg.cchReturnPathLength = sizeof(szCounterPath);
	brwDlgCfg.dwDefaultDetailLevel = PERF_DETAIL_WIZARD;
	brwDlgCfg.szDialogBoxCaption = "PDH Chooser";
	
	szCounterPath[0] = szCounterPath[1] = 0;

	// Bring up the counter browsing dialog
	if ( ERROR_SUCCESS != PdhBrowseCounters( &brwDlgCfg ) )
		return;
	
	for (char *p = szCounterPath; *p; p = p + strlen(p) + 1) {
		printf("Adding %s\n", p);
		AddCounter(p);
	}
}

void PDH::AddObjects(char *objsearch, char *countersearch, char *ifsearch) {
	
	char objs[4096];
	unsigned long objsize = sizeof(objs);

	if (ERROR_SUCCESS != PdhEnumObjects(NULL, NULL, objs, &objsize, PERF_DETAIL_WIZARD, FALSE)) return;

	// loop over strings
	for (char *obj = objs; *obj; obj += strlen(obj) + 1) {

		if (!strstr(obj, objsearch)) {
			continue;
		}

		char counterlist[4096];
		unsigned long csize = sizeof(counterlist);
		char instancelist[4096];
		unsigned long isize = sizeof(instancelist);
		char temppath[4096];
		
		if (ERROR_SUCCESS != PdhEnumObjectItems(NULL, NULL, obj, 
												counterlist, &csize,
												instancelist, &isize,
												PERF_DETAIL_WIZARD, 0)) continue;

		//printf("%s ====\n", obj);
		for (char *counter = counterlist; *counter; counter += strlen(counter) + 1) {
			if (countersearch && !strstr(counter, countersearch)) continue;
			//printf(" Counter %s\n", counter);

			if (!*instancelist) {
				sprintf(temppath, "\\%s\\%s", obj, counter);
				AddCounter(temppath);
				continue;
			}

			for (char *instance = instancelist; *instance; instance += strlen(instance) + 1) {
				if (ifsearch && !strstr(instance, ifsearch)) continue;
				sprintf(temppath, "\\%s(%s)\\%s", obj, instance, counter);
				AddCounter(temppath);
			}
		}
	}
}

void PDH::Tick() {
	int count = 0;
	for (PDHList *l = head; l; l = l->next) {
		count += l->q.GetData();
	}
}
