from dataclasses import dataclass
import matplotlib.pyplot as plt
import numpy as np
import os

emitterCount = 2
frameCount = 3
exePath = ".\\Build\\bin\\Release-windows-x86_64\\VulkanProject\\VulkanProject.exe"

def plotLineGraphs(arrays, xlim, title):
    fig = plt.figure()

    for array in arrays:
        plt.plot(array)

    plt.xlim(0, xlim)
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

def startApplication(particleCount, multipleQueues):
    command = "{} {} {} {} {}".format(exePath, emitterCount, frameCount, int(multipleQueues), float(particleCount))
    print("Executing following command:")
    print(command)
    err = os.system(command)
    print("System finished with code: " + str(err))
    return

def main():
    endParticleCount        = 100000
    particleCountIncrement  = 100000

    testCount = int(endParticleCount / particleCountIncrement)
    averageUpdateTimes  = [None] * 2
    totalExecutionTimes = [None] * 2

    particleCount = particleCountIncrement

    for multipleQueues in [False, True]:
        averageUpdateTimes[int(multipleQueues)]     = [None] * testCount
        totalExecutionTimes[int(multipleQueues)]    = [None] * testCount

        for testNr in range(testCount):
            startApplication(particleCount, multipleQueues)
            results         = readResultsFile()
            emitterTimes    = results["EmitterTimes"]

            averageUpdateTimes[int(multipleQueues)][testNr]  = calculateAverageUpdateTime(emitterTimes)
            totalExecutionTimes[int(multipleQueues)][testNr] = results["TotalTime"]

            particleCount += particleCountIncrement

    plotLineGraphs(averageUpdateTimes, endParticleCount, "Average Update Times")
    plotLineGraphs(totalExecutionTimes, endParticleCount, "Total Execution Times")


if __name__ == "__main__":
    main()
