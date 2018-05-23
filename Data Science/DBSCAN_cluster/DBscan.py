import math
import sys

def readInputFile(fileName):
    datas = []
    inputfile = open("./"+fileName, 'r')
    lines = inputfile.readlines()
    for line in lines:
        data = line[:-1].split('\t')
        datas.append(data)

    inputfile.close()
    return datas


# euclidean distance formula
def euclideanDistance(x, p):
    point = 0
    distance = math.sqrt(math.pow(float(datas[x][1]) - float(datas[p][1]), 2) +
                         math.pow(float(datas[x][2]) - float(datas[p][2]), 2))

    return distance


# find out neighbors of point within given epsilon
def checkNeighbor(datas, i, eps):
    neighbor = []

    for p in range (len(datas)):
        if euclideanDistance(i, p) <= eps:
            neighbor.append(p)

    return neighbor


## collect datas for each cluster
def DBScan(datas, n, eps, minPts):
    ## -2: undefined
    ## -1: noise
    checkCluster = [-2 for undefined in range (len(datas))]
    numCluster = 0

    for i in range (len(datas)):
        # skip defined datas
        if checkCluster[i] != -2:
            continue

        # datas that are near the current data (= neighbor)
        neighbor = checkNeighbor(datas, i, eps)
        
        ## skip noise datas
        if len(neighbor) < minPts:
            checkCluster[i] = -1
            continue
        
        checkCluster[i] = numCluster

        ## set cluster number of neighbors
        for id in neighbor:
            checkCluster[id] = numCluster

        # search density-reachable in each neighbor
        while len(neighbor) > 0:
            currentNeighbor = neighbor[0]
            recurNeighbor = checkNeighbor(datas, currentNeighbor, eps)

            # core point
            if len(recurNeighbor) >= minPts:
                for i in range (len(recurNeighbor)):
                    point = recurNeighbor[i]
                    if checkCluster[point] == -2 or checkCluster[point] == -1:
                        neighbor.append(point)
                        checkCluster[point] = numCluster

            del neighbor[0]
            
        numCluster += 1

    return checkCluster, numCluster
        

def writeFile(fileName, checkCluster, numCluster, datas):
    for i in range (len(checkCluster)):
        if checkCluster[i] == numCluster:
            word = ""
            word += "%s\n" % datas[i][0]
            fileName.write(word)

    return fileName
        

def excess(clusterLabel, num, n):
    count = {}
    for c in range (num):
        count[c] = clusterLabel.count(c)

    key = list(count.keys())
    value = list(count.values())    

    # delete the small size of cluster
    for i in range (num - n):
        minimum = min(value)
        minIndex = value.index(minimum)
        minLabelIndex = key[minIndex]
        del value[minIndex]
        del key[minIndex]

    return key


## main

# Check the command line arguments
if len(sys.argv) != 5:
    print('''Please fill in the command form.
Executable_file minimum_support inputfile outputfile''')
    exit()

inputFile = sys.argv[1]
n = int(sys.argv[2])
eps = int(sys.argv[3])
minPts = int(sys.argv[4])

split = inputFile.split('.')[0]

datas = readInputFile(inputFile)
clusterLabel, num = DBScan(datas, n, eps, minPts)

# when the number of cluster exceeds given n
if num > n:
    key = excess(clusterLabel, num, n)

    for numCluster in range (n):
        fileName = split + '_cluster_' + str(numCluster) + '.txt'
        outputFile = open(fileName, 'w')
        writeFile(outputFile, clusterLabel, key[numCluster], datas)
        outputFile.close()

# not exceed
for numCluster in range (n):
    fileName = split + '_cluster_' + str(numCluster) + '.txt'
    outputFile = open(fileName, 'w')
    writeFile(outputFile, clusterLabel, numCluster, datas)
    outputFile.close()

print('DBscan done')

