#ifndef __PROGTEST__
#include "progtest_solver.h"
#include "sample_tester.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cfloat>
#include <chrono>
#include <climits>
#include <cmath>
#include <compare>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <pthread.h>
#include <queue>
#include <semaphore.h>
#include <set>
#include <stack>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>
using namespace std;
#endif /* __PROGTEST__ */

//-------------------------------------------------------------------------------------------------------------------------------------------------------------
class CProblemWrap {
private:
	bool solved;

public:
	APolygon polygon;
	CProblemWrap(const APolygon &pol) : polygon(pol) {
		solved = false;
	}
	void markSolved() {
		solved = true;
	}
	bool isSolved() const {
		return solved;
	}
};

class CPackWrap {
public:
	AProblemPack m_pack;
	vector<shared_ptr<CProblemWrap>> m_cntVec;
	vector<shared_ptr<CProblemWrap>> m_minVec;

	CPackWrap(const AProblemPack &pack) : m_pack(pack) {
		for (auto polygon : pack->m_ProblemsCnt) {
			m_cntVec.emplace_back(make_shared<CProblemWrap>(polygon));
		}
		for (auto polygon : pack->m_ProblemsMin) {
			m_minVec.emplace_back(make_shared<CProblemWrap>(polygon));
		}
	}
	bool isSolved() const {
		for (auto problem : m_cntVec) {
			if (!problem->isSolved()) {
				return false;
			}
		}
		for (auto problem : m_cntVec) {
			if (!problem->isSolved()) {
				return false;
			}
		}
		return true;
	}
};

class CCompanyWrap {
public:
	ACompany m_company;
	queue<shared_ptr<CPackWrap>> m_outputQueue;
	mutex m_qMut;
	bool m_running;

	CCompanyWrap(const ACompany &company) : m_company(company) {
		m_running = true;
	}
};

class COptimizer {
public:
	static bool usingProgtestSolver(void) {
		return true;
	}
	static void checkAlgorithmMin(APolygon p) {
		// dummy implementation if usingProgtestSolver() returns true
	}
	static void checkAlgorithmCnt(APolygon p) {
		// dummy implementation if usingProgtestSolver() returns true
	}
	void start(int threadCount);
	void stop(void);
	void addCompany(ACompany company);

private:
};
// TODO: COptimizer implementation goes here
//-------------------------------------------------------------------------------------------------------------------------------------------------------------
#ifndef __PROGTEST__
int main(void) {
	COptimizer optimizer;
	ACompanyTest company = std::make_shared<CCompanyTest>();
	optimizer.addCompany(company);
	optimizer.start(4);
	optimizer.stop();
	if (!company->allProcessed())
		throw std::logic_error("(some) problems were not correctly processsed");
	return 0;
}
#endif /* __PROGTEST__ */
