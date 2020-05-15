from dataclasses import dataclass
import matplotlib.pyplot as plt
import numpy as np
import os

emitterCount = 8
frameCount = 100
exePath = ".\\Build\\bin\\Release-windows-x86_64\\VulkanProject\\VulkanProject.exe"

def plotLineGraphs(yArrays, xlim, labels, title):
    fig = plt.figure()

    for plotNr in range(len(yArrays)):
        xStepLength = int(xlim / len(yArrays[plotNr]))
        xticks = range(xStepLength, xlim + xStepLength, xStepLength)

        plt.plot(xticks, yArrays[plotNr])

    plt.legend(labels)
    plt.title(title)
    plt.ylabel("Time (ms)")
    plt.xlabel("Particle Count")
    fig.canvas.set_window_title(title)

    plt.show()

# Each emitter has frameCount * 2 timestamps
def fillEmitterTimes(emitterValues, emitterTimes, timestampToMilli):
    for emitterIdx in range(len(emitterValues)):
        minTimestamp = emitterValues[emitterIdx][0]
        for timestampIdx in range(0, len(emitterValues[emitterIdx]) - 1, 2):
            timestamp1 = emitterValues[emitterIdx][timestampIdx + 1]
            timestamp0 = emitterValues[emitterIdx][timestampIdx]

            if timestamp1 < timestamp0:
                print("1st larger than 2nd: {} vs {}".format(timestamp0, timestamp1))

            duration    = timestamp1 - timestamp0
            duration    *= timestampToMilli
            startTime   = timestamp0 - minTimestamp
            startTime   *= timestampToMilli
            emitterTimes[emitterIdx].append((startTime, duration))

def readResultsFile():
    emitterTimes = []
    for _ in range(emitterCount):
        emitterTimes.append([])
    emitterValues   = [[]] * emitterCount

    # Read values
    file = open("results.txt", "rb")
    timestampToMilli = np.frombuffer(file.read(8), dtype=np.double)[0]
    totalTime = np.frombuffer(file.read(8), dtype=np.double)[0]
    for emitterIdx in range(emitterCount):
        emitterValues[emitterIdx] = np.frombuffer(file.read(8 * 2 * frameCount), dtype=np.uint64)

    fillEmitterTimes(emitterValues, emitterTimes, timestampToMilli)

    results = {
        "TotalTime": totalTime,
        "EmitterTimes": emitterTimes
    }

    return results

def calculateAverageUpdateTime(emitterTimes):
    # Emitter times:
    #   - Emitter
    #       - Tuples of startTime and duration
    totalDuration = 0.0
    for emitter in emitterTimes:
        for time in emitter:
            totalDuration += time[1]

    average = totalDuration / (len(emitterTimes) * len(emitterTimes[0]))
    return average

def startApplication(particleCount, multipleQueues, multipleFamilies, useComputeQueue):
    command = "{} {} {} {} {} {} {}".format(exePath, emitterCount, frameCount, int(multipleQueues), float(particleCount), int(multipleFamilies), int(useComputeQueue))
    print("Executing following command:")
    print(command)
    err = os.system(command)
    print("System finished with code: " + str(err))
    return

def main():
    endParticleCount        = 100000
    particleCountIncrement  = 10000
    useComputeQueue = True

    testCount = int(endParticleCount / particleCountIncrement)
    averageUpdateTimes  = [None] * 3
    totalExecutionTimes = [None] * 3

    particleCount = particleCountIncrement

    multipleQueuesTest   = [False, True, True]
    multipleFamiliesTest = [False, False, True]

    for test in range(3):
        averageUpdateTimes[int(test)]     = [None] * testCount
        totalExecutionTimes[int(test)]    = [None] * testCount

        for testNr in range(testCount):
            startApplication(particleCount, multipleQueuesTest[test], multipleFamiliesTest[test], useComputeQueue)
            results         = readResultsFile()
            emitterTimes    = results["EmitterTimes"]

            averageUpdateTimes[int(test)][testNr]  = calculateAverageUpdateTime(emitterTimes)
            totalExecutionTimes[int(test)][testNr] = results["TotalTime"]

            particleCount += particleCountIncrement

    plotLabels = ["Single Queue", "Multiple Queues", "Multiple Families"]
    plotLineGraphs(averageUpdateTimes, endParticleCount, plotLabels, "Average Update Times")
    plotLineGraphs(totalExecutionTimes, endParticleCount, plotLabels, "Total Execution Times")


if __name__ == "__main__":
    main()
