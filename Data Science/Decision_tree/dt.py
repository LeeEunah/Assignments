from math import log


def readData(fileName):
    datas = []
    inputfile = open("./"+fileName, 'r')
    firstLine = inputfile.readline()
    attributes = firstLine[:-1].split('\t')
    
    lines = inputfile.readlines()
    for line in lines:
        divide = line[:-1].split('\t')
        datas.append(divide)

    inputfile.close()
    return datas, attributes


def testTree(tree, data, attributes, index, outputFile):
    if type(tree) == str:
        file = open("./" + outputFile, 'a')
        file.write('\n')
        for value in data:
            file.write(value + '\t')
        file.write(tree)
        file.close()

        return

    else:
        key = list(tree.keys())
        bestAttribute = key[0]


        if bestAttribute in attributes:

            index = attributes.index(bestAttribute)
            Tree = testTree(tree[bestAttribute], data, attributes, index, outputFile)
    
        else:
            value = data[index]
            Tree = testTree(tree[value], data, attributes, index, outputFile)
    return classLabel

            
##    classLabel = []
##    print('@@@train: ', trainTree)
##    for i in range (len(data)):
##        print('data: ', data[i])
##        if type(trainTree) == str:
##            print('end')
###            print('trainTree: ', trainTree)
##            classLabel.append(trainTree)
##            return
##
##        else:    
##            key = list(trainTree.keys())
##            bestAttribute = key[0]
##                
##
##            if bestAttribute in attributes:
##                print("//attri: ", bestAttribute)
##                index = attributes.index(bestAttribute)
##                Tree = testTree(trainTree[bestAttribute], data, attributes, index)
##
##            else:
##                value = data[i][index]
##                print("//value: ", value)
##                Tree = testTree(trainTree[value], data, attributes, index)
##
##    
##    return classLabel



def createTree(datas, attributes):
    bestGain, bestIndex = information_gain(datas, attributes)
#    print('bestIndex: ', bestIndex)
    bestAttribute = attributes[bestIndex]
#    print('bestAttri: ', bestAttribute)
    Tree = {bestAttribute : {}}
    stopTree = {}
#    print('datas: ', datas)
#    kindOfValue = set
    kindOfValue = set([data[bestIndex] for data in datas])
    listKind = list(kindOfValue)

    del (attributes[bestIndex])

    i = 0
#    print('list :' ,listKind)
    for value in listKind:
        if bestGain > 0:
            subData, classLabel = pickAttribute2(datas, bestIndex, value)
            for data in subData:
                del data[bestIndex]
            subAttribute = attributes[:]
            if len(set(classLabel)) == 1:
#                print('homo')
                strAnswer = list(set(classLabel))
                stopTree[listKind[i]] = strAnswer[0]
            i = i + 1

            if value in stopTree.keys():
#                print('stop')
                Tree[bestAttribute][value] = stopTree[value]
                continue

            Tree[bestAttribute][value] = createTree(subData, subAttribute)
    return Tree
    

def entropy(datas):
    dataSize = len(datas)
    classLabel = {}
    entropy = 0.0

    for data in datas:
        currentLabel = data[-1]
        if currentLabel not in classLabel.keys():
            classLabel[currentLabel] = 0
        classLabel[currentLabel] += 1

    for label in classLabel:
        probability = float(classLabel[label]/dataSize)
        entropy -= probability * log(probability, 2)
    return entropy

    
def information_gain(datas, attributes):
    dataSize = len(datas)
    gainList = []
    rmAttri = attributes[:-1]

#    print('attribute: ', attributes)
    for index in range (len(rmAttri)):
        attributeCount = {}
        weightedEntropy = 0.0
        splitInfo = 0.0
        for data in datas:
            currentAttribute = data[index]

            if currentAttribute not in attributeCount.keys():
                attributeCount[currentAttribute] = 0
            attributeCount[currentAttribute] += 1
    
#        print('attributeCount: ' , attributeCount)
        for attribute in attributeCount:
            probability = float(attributeCount[attribute]/dataSize)
            partOfData = pickAttribute(datas,index, attribute)
            weightedEntropy += probability * entropy(partOfData)
    #        print('attribute: ', attribute)
            gain = entropy(datas) - weightedEntropy
            
        
            if len(attributeCount) > 1:
                splitInfo -= probability * log(probability, 2)
                gain = gain / splitInfo
        gainList.append(gain)

    # best
    bestGain = 0.0
    for index in range (len(gainList)):
        if bestGain < gainList[index]:
            bestGain = gainList[index]
            gainIndex = index

#    print('gainInit, gainIndex: ', gainInit, gainIndex)
    return bestGain, gainIndex


def pickAttribute(datas, index, attribute):
    pickData = []
    for data in datas:
        if data[index] == attribute:
            pickData.append(data[:])
    return pickData 


def pickAttribute2(datas, index, value):
    pickData = []
    classLabel = []
#    print('value: ', value)
    for data in datas:
        if data[index] == value:
            classLabel.append(data[-1])
#            del data[index]
            pickData.append(data[:])
#    print('pinckData: ', pickData)
    return pickData, classLabel


# main
datas, attributes = readData("dt_train.txt")

outputFile = "answer1.txt"
file = open("./" + outputFile, 'w')
for attribute in attributes:
    file.write(attribute + '\t')
file.close()

Tree = createTree(datas, attributes)

print('Tree: ', Tree)
datas, attributes = readData("dt_test.txt")

#    print("!!!!!!: ", data)
classLabel = []
for data in datas:
    test = testTree(Tree, data, attributes, 0, outputFile)
