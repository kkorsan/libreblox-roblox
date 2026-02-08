#pragma once

#include "Replicator.h"
#include "Players.h"
#include "util/RbxStringTable.h"
#include "util/xxhash.h"
#include "util/ObscureValue.h"
#include "v8datamodel/Stats.h"
#include "v8datamodel/HackDefines.h"

LOGGROUP(NetworkStepsMultipliers)
DYNAMIC_FASTINTVARIABLE(MaxDataStepsPerCyclic, 5)
DYNAMIC_FASTINTVARIABLE(MaxDataStepsAccumulated, 15)
DYNAMIC_FASTINTVARIABLE(MaxClusterSendStepsPerCyclic, 5)
DYNAMIC_FASTINTVARIABLE(MaxClusterSendStepsAccumulated, 15)
DYNAMIC_FASTINTVARIABLE(MaxDataOutJobScaling, 10)

namespace RBX { namespace Network {

class Replicator::SendDataJob : public ReplicatorJob
{
	const ObscureValue<float> dataSendRate;
	const PacketPriority packetPriority;
	unsigned int countdownToXxhashCheck;

public:
	SendDataJob(Replicator& replicator)
		:ReplicatorJob("Replicator SendData", replicator, DataModelJob::DataOut)
		,dataSendRate(replicator.settings().getDataSendRate())
		,packetPriority(replicator.settings().getDataSendPriority())
		,countdownToXxhashCheck(0)
	{
		cyclicExecutive = true;
		cyclicPriority = CyclicExecutiveJobPriority_Network_ProcessOutgoing;
	}

private:
	virtual Time::Interval sleepTime(const Stats& stats)
	{
		// TODO: If Replicator::pendingItems is empty then sleep
		return computeStandardSleepTime(stats, dataSendRate);
	}

	virtual Error error(const Stats& stats);
	virtual TaskScheduler::StepResult stepDataModelJob(const Stats& stats)
	{
		if(replicator){
			try
			{
				if (TaskScheduler::singleton().isCyclicExecutive() && cyclicExecutive)
				{
					float stepsToDo = updateStepsRequiredForCyclicExecutive(stats.timespanSinceLastStep.seconds(),
																			replicator->settings().getDataSendRate(),
																			(float)DFInt::MaxDataStepsPerCyclic,
																			(float)DFInt::MaxDataStepsAccumulated);

					stepsToDo = std::max<float>(1.0, stepsToDo);
					stepsToDo *= std::min<int>(10, replicator->highPriorityPendingItems.size() + replicator->pendingItems.size() + 1) / 2.0;
					float truncatedStepsToDo = std::min<float>((float)DFInt::MaxDataOutJobScaling, stepsToDo);
					for (int i = 0; i < (int)truncatedStepsToDo; i++)
					{
						replicator->dataOutStep();
					}
				}
				else
				{
					replicator->dataOutStep();
				}
			}
			catch (RBX::base_exception& e)
			{
				const char* what = e.what();
				RBX::StandardOut::singleton()->printf(RBX::MESSAGE_ERROR, "SendData: %s", what ? what : "empty error string");
				return TaskScheduler::Done;
			}
			return TaskScheduler::Stepped;
		}
		return TaskScheduler::Done;
	}
};

class Replicator::SendClusterJob : public ReplicatorJob
{
	ObscureValue<float> dataSendRate;
	PacketPriority packetPriority;

public:
	SendClusterJob(Replicator& replicator)
		:ReplicatorJob("Replicator SendCluster", replicator, DataModelJob::DataOut)
		,dataSendRate(replicator.settings().getDataSendRate())
		,packetPriority(replicator.settings().getDataSendPriority())
	{
		cyclicExecutive = true;
		cyclicPriority = CyclicExecutiveJobPriority_Network_ProcessOutgoing;
	}

private:
	virtual Time::Interval sleepTime(const Stats& stats)
	{
		// TODO: If Replicator::pendingItems is empty then sleep
		return computeStandardSleepTime(stats, dataSendRate);
	}

	virtual Error error(const Stats& stats);

	virtual TaskScheduler::StepResult stepDataModelJob(const Stats& stats)
	{
		if(replicator){
			try
			{
				if (TaskScheduler::singleton().isCyclicExecutive() && cyclicExecutive)
				{
					double stepsRequired = updateStepsRequiredForCyclicExecutive(stats.timespanSinceLastStep.seconds(),
																				replicator->settings().getDataSendRate(),
																				(float)DFInt::MaxClusterSendStepsPerCyclic,
																				(float)DFInt::MaxClusterSendStepsAccumulated);
					float multiplier = 1.0;
					if (int maxBytesSend = replicator->getAdjustedMtuSize())
					{
						// assuming there's just one cluster for now
						int deltas = replicator->getApproximateSizeOfPendingClusterDeltas();

						if (deltas > maxBytesSend)
						{
							multiplier *= deltas;
							multiplier /= maxBytesSend;
						}
					}
					if (replicator->getApproximateSizeOfPendingClusterDeltas() > 0.0f)
					{
						stepsRequired = std::max<float>(1.0, stepsRequired);
					}
					float sendDataSteps = std::min<float>((float)DFInt::MaxDataOutJobScaling, stepsRequired * multiplier);

					for (int i = 0; i < (int)sendDataSteps; i++)
					{
						replicator->clusterOutStep();
					}
				}
				else
				{
					replicator->clusterOutStep();
				}
			}
			catch (RBX::base_exception& e)
			{
				const char* what = e.what();
				RBX::StandardOut::singleton()->printf(RBX::MESSAGE_ERROR, "SendCluster: %s", what ? what : "empty error string");
				return TaskScheduler::Done;
			}
			return TaskScheduler::Stepped;
		}
		return TaskScheduler::Done;
	}
};


}}
