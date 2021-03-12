/*
 * Round-robin Scheduling Algorithm
 * SKELETON IMPLEMENTATION -- TO BE FILLED IN FOR TASK (1)
 */

/*
 * STUDENT NUMBER: s2002579 
 */
#include <infos/kernel/sched.h>
#include <infos/kernel/thread.h>
#include <infos/kernel/log.h>
#include <infos/util/list.h>
#include <infos/util/lock.h>

using namespace infos::kernel;
using namespace infos::util;

/**
 * A round-robin scheduling algorithm
 */
class RoundRobinScheduler : public SchedulingAlgorithm
{
public:
	/**
	 * Returns the friendly name of the algorithm, for debugging and selection purposes.
	 */
	const char* name() const override { return "rr"; }

	/**
	 * Called when a scheduling entity becomes eligible for running.
	 * @param entity
	 */
	void add_to_runqueue(SchedulingEntity& entity) override
	{
	    UniqueIRQLock l;
		runqueue.enqueue(&entity);
	}

	/**
	 * Called when a scheduling entity is no longer eligible for running.
	 * @param entity
	 */
	void remove_from_runqueue(SchedulingEntity& entity) override
	{
		UniqueIRQLock l;
		runqueue.remove(&entity);
	}

	/**
	 * Called every time a scheduling event occurs, to cause the next eligible entity
	 * to be chosen.  The next eligible entity might actually be the same entity, if
	 * e.g. its timeslice has not expired.
	 */
	SchedulingEntity *pick_next_entity() override
	{
		UniqueIRQLock l;
		/**
		* If there is no entity in the queue, return null.
		*/
		if (runqueue.count() == 0) return NULL;
		/**
		* If there is only one entity in the queue, return this entity.
		*/
		if (runqueue.count() == 1) return runqueue.first();
		/**
		* If there are two or more than two entities in the queue, 
		* take it out and put into the rear of the queue.
		* And then return the first entity.
		*/
		SchedulingEntity *next_entity = runqueue.dequeue();
		runqueue.enqueue(next_entity);
		return next_entity;
		
	}

private:
	// A list containing the current runqueue.
	List<SchedulingEntity *> runqueue;
};

/* --- DO NOT CHANGE ANYTHING BELOW THIS LINE --- */

RegisterScheduler(RoundRobinScheduler);
