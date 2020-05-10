from dataclasses import dataclass
import matplotlib.pyplot as plt
import numpy as np

emitterCount = 2
frameCount = 3
multipleQueues = True

def visualizeTimeline(emitterTimes):
    yTickLabels = [""] * len(emitterTimes)

    barHeight = 2

    # Vertical space between each emitter's bar
    verticalSpacing = 1
    _, ax = plt.subplots()

    for emitterIdx in range(len(emitterTimes)):
        yTickLabels[emitterIdx] = "Emitter #{}".format(emitterIdx)

        barStartY = verticalSpacing * (emitterIdx + 1) + emitterIdx * barHeight

        #data = {emitterTimes[emitterIdx][0], emitterTimes[emitterIdx][1]}
        data = emitterTimes[emitterIdx]
        ax.broken_barh(data, (barStartY, barHeight))

    #ax.set_ylim(5, 35)
    #ax.set_xlim(0, 200)
    ax.set_xlabel('Execution Time (ms)')
    #ax.set_yticks([15, 25])

    ax.set_yticklabels(yTickLabels)
    ax.grid(True)

    plt.show()

# Each emitter has frameCount * 2 timestamps

# def getMinMaxTimestamps(emitterValues):
#     minTimestamp = np.iinfo(np.uint64).max
#     maxTimestamp = 0
#     maxDelta = 0

#     for emitterIdx in range(len(emitterValues)):
#         for i in range(emitterCount):
#             delta = emitterValues[emitterIdx][frameCount - 1] - emitterValues[i][0]
#             if delta > maxDelta:
#                 maxDelta = delta
#                 minTimestamp = emitterValues[i][0]
#                 maxTimestamp = emitterValues[emitterIdx][frameCount - 1]

#     return (minTimestamp, maxTimestamp)

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

def main():
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

    # minMaxGPU = getMinMaxTimestamps(emitterValues)
    # totalTime = (minMaxGPU[1] - minMaxGPU[0]) * timestampToMilli
    # totalTime = (emitterValues[0][emitterCount - 1] - emitterValues[0][0]) * timestampToMilli

    fillEmitterTimes(emitterValues, emitterTimes, timestampToMilli)

    print(emitterTimes[0])
    print("Total time: " + str(totalTime))
    visualizeTimeline(emitterTimes)


if __name__ == "__main__":
    main()
