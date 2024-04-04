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
class CProblemWrap;

class CPackWrap {
public:
	AProblemPack m_pack;
	vector<shared_ptr<CProblemWrap>> m_cntVec;
	vector<shared_ptr<CProblemWrap>> m_minVec;

	mutex m_mut;
	condition_variable m_cond;
	size_t m_ToSolve;

	CPackWrap(const AProblemPack &pack) : m_pack(pack) {
		for (auto polygon : pack->m_ProblemsCnt) {
			m_cntVec.emplace_back(make_shared<CProblemWrap>(polygon, this));
		}
		for (auto polygon : pack->m_ProblemsMin) {
			m_minVec.emplace_back(make_shared<CProblemWrap>(polygon, this));
		}
		m_ToSolve = m_cntVec.size() + m_minVec.size();
	}

	bool isSolved() {
		return m_ToSolve == 0;
	}

	void markSolved() {
		lock_guard guard(m_mut);
		if (m_ToSolve == 0) {
			throw logic_error("Can't mark solved more than there is"); // todo: if good, remove
		}

		--m_ToSolve;
		if (m_ToSolve == 0) {
			m_cond.notify_all();
		}
	}
};

class CProblemWrap {
private:
	CPackWrap *m_parent;
	size_t m_mark;

public:
	APolygon polygon;
	CProblemWrap(const APolygon &pol, CPackWrap *parent) : polygon(pol) {
		m_parent = parent;
		m_mark = 0;
	}
	void markSolved() {
		++m_mark;
		m_parent->markSolved();
	}
};

class CCompanyWrap {
public:
	ACompany m_company;
	queue<shared_ptr<CPackWrap>> m_outputQueue;
	mutex m_qMut;
	sem_t m_sem;

	CCompanyWrap(const ACompany &company) : m_company(company) {
		sem_init(&m_sem, 0, 0);
	}
	~CCompanyWrap() {
		sem_destroy(&m_sem);
	}
};

class CSolverWrap {
public:
	AProgtestSolver m_solver;
	vector<shared_ptr<CProblemWrap>> m_solving;
	CSolverWrap(const AProgtestSolver &solver) : m_solver(solver) {}
};

class COptimizer {
private:
	int m_threadCount;

	queue<shared_ptr<CSolverWrap>> m_SolverQueue;
	sem_t m_SolverSem;
	mutex m_SolverQueueMut;

	shared_ptr<CSolverWrap> m_CntSolver;
	mutex m_CntSolverMut;
	shared_ptr<CSolverWrap> m_MinSolver;
	mutex m_MinSolverMut;

	vector<shared_ptr<CCompanyWrap>> m_Companies;
	vector<thread> m_inputThreads;
	vector<thread> m_outputThreads;
	vector<thread> m_workerThreads;

	void addProblemCnt(const shared_ptr<CProblemWrap> &toSolve) {
		lock_guard guard(m_CntSolverMut);
		m_CntSolver->m_solver->addPolygon(toSolve->polygon); // todo: make more safe
		m_CntSolver->m_solving.push_back(toSolve);
		if (!m_CntSolver->m_solver->hasFreeCapacity()) {
			lock_guard guard2(m_SolverQueueMut);
			m_SolverQueue.push(m_CntSolver);
			m_CntSolver = make_shared<CSolverWrap>(createProgtestCntSolver());
			sem_post(&m_SolverSem);
		}
	}

	void addProblemMin(const shared_ptr<CProblemWrap> &toSolve) {
		lock_guard guard(m_MinSolverMut);
		m_MinSolver->m_solver->addPolygon(toSolve->polygon); // todo: make more safe
		m_MinSolver->m_solving.push_back(toSolve);
		if (!m_MinSolver->m_solver->hasFreeCapacity()) {
			lock_guard guard2(m_SolverQueueMut);
			m_SolverQueue.push(m_MinSolver);
			m_MinSolver = make_shared<CSolverWrap>(createProgtestMinSolver());
			sem_post(&m_SolverSem);
		}
	}

	void inputFunc(shared_ptr<CCompanyWrap> company) {
		AProblemPack pack;
		while ((bool)(pack = company->m_company->waitForPack())) {
			// todo: add wait if queues are too full?
			shared_ptr<CPackWrap> packWrap = make_shared<CPackWrap>(pack);
			for (auto &cntProblem : packWrap->m_cntVec) {
				addProblemCnt(cntProblem);
			}
			for (auto &minProblem : packWrap->m_minVec) {
				addProblemMin(minProblem);
			}
			{
				lock_guard guard(company->m_qMut);
				company->m_outputQueue.push(packWrap);
			}
			sem_post(&company->m_sem);
		}
		sem_post(&company->m_sem);
	}

	void outputFunc(shared_ptr<CCompanyWrap> company) {
		while (true) {
			sem_wait(&company->m_sem);
			if (company->m_outputQueue.empty()) {
				break;
			}
			shared_ptr<CPackWrap> pack(nullptr);
			{
				lock_guard guard(company->m_qMut);
				pack = company->m_outputQueue.front();
			}
			{
				unique_lock guard(pack->m_mut);
				pack->m_cond.wait(guard, [&pack]() { return pack->isSolved(); });
			}
			{
				lock_guard guard(company->m_qMut);
				company->m_company->solvedPack(pack->m_pack);
				company->m_outputQueue.pop();
			}
		}
	}

	void workerFunc() {
		while (true) {
			// get solver from queue
			sem_wait(&m_SolverSem);
			shared_ptr<CSolverWrap> solver;
			{
				lock_guard guard(m_SolverQueueMut);
				if (m_SolverQueue.empty()) {
					break;
				}
				solver = m_SolverQueue.front();
				m_SolverQueue.pop();
			}
			// solve it
			solver->m_solver->solve();
			// mark elements as solved
			for (auto &pol : solver->m_solving) {
				pol->markSolved();
			}
			// todo: notify output thread if pack solved?
		}
	}

	void workerFunc() {
		// wait until at least one queue is non-empty
		// chose which problem to solve
		// get all the polygons from problem queue
		// fill solver until full or something idk
		// solve
		// end loop if marked for end
	}

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
	void addCompany(ACompany company) {
		m_Companies.emplace_back(make_shared<CCompanyWrap>(company));
	}
	void start(int threadCount) {
		// Init
		m_threadCount = threadCount;
		m_CntSolver = make_shared<CSolverWrap>(createProgtestCntSolver());
		m_MinSolver = make_shared<CSolverWrap>(createProgtestMinSolver());
		sem_init(&m_SolverSem, 0, 0);
		// add threads
		for (auto &company : m_Companies) {
			m_inputThreads.emplace_back(&COptimizer::inputFunc, this, company);
			m_outputThreads.emplace_back(&COptimizer::outputFunc, this, company);
		}
		for (int i = 0; i < threadCount; ++i) {
			m_workerThreads.emplace_back(&COptimizer::workerFunc, this);
		}
	}
	void stop(void) {
		for (auto &input : m_inputThreads) {
			input.join();
		}
		for (auto &worker : m_workerThreads) {
			worker.join();
		}
		for (auto &output : m_outputThreads) {
			output.join();
		}
		sem_destroy(&m_SolverSem);
	}
};
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
