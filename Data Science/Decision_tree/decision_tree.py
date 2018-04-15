from math import log


def tree(datas, attributes):
    bestGain = 0.0
    bestIndex = 0

# check best gain ratio among the attributes
    # Stop tree
    # when all of the values of label are equal
    if len(attributes) <= 1:
        return

    else:
        bestGain, bestIndex = selectBestAttribute(datas, attributes)
    
        bestAttribute = attributes[bestIndex]
        Tree = {bestAttribute : {}}
        
# gathering the label of attribute without duplicating
        kindOfValue = set([label[bestIndex] for label in datas])
        listKind = list(kindOfValue)
        stopTree = {}
        answers = []
        for value in kindOfValue:
            answer = []
            for data in datas:
                if data[bestIndex] == value:
                    answer.append(data[-1])
            answers.append(answer)

        i = 0
        for answer in answers:
            if len(set(answer)) == 1:
                strAnswer = list(set(answer))
                
                stopTree[listKind[i]] = strAnswer[0]
            i = i + 1
            
        
        del (attributes[bestIndex])


# recursive
        for value in listKind:
            if value in stopTree.keys():       
                Tree[bestAttribute][value] = stopTree[value]
                continue
            subAttribute = attributes[:]
            subData = datas[:]

            subData = pickAttribute2(datas, bestIndex, value)

            Tree[bestAttribute][value] = tree(subData, subAttribute)

        return Tree

        

    


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

# the number of label in data
def pickAttribute(datas, index, attribute):
    pickData = []
    for data in datas:
        if data[index] == attribute:
            pickData.append(data[:])
    return pickData            

def pickAttribute2(datas, index, attribute):
    pickData = []
    for data in datas:
        if data[index] == attribute:
            del data[index]
            pickData.append(data[:])
    return pickData 

def information_gain(datas, index):
    dataSize = len(datas)
    attributeCount = {}
    weightedEntropy = 0.0
    splitInfo = 0.0

    for data in datas: 
        currentAttribute = data[index]
        if currentAttribute not in attributeCount.keys():
            attributeCount[currentAttribute] = 0
        attributeCount[currentAttribute] += 1

    for attribute in attributeCount:
        probability = float(attributeCount[attribute]/dataSize)
        partOfData = pickAttribute(datas,index, attribute)
        weightedEntropy += probability * entropy(partOfData)

        gain = entropy(datas) - weightedEntropy

        if len(attributeCount) > 1:
            splitInfo -= probability * log(probability, 2)

            gain = gain / splitInfo

    return gain


def selectBestAttribute(datas, attributes):
    bestGain = 0.0
    for i in range (len(attributes)-1):
        gainRatio = information_gain(datas, i)

        if bestGain < gainRatio:
            bestGain = gainRatio
            bestIndex = i

    
    return bestGain, bestIndex




# main
datas, attributes = readData("dt_train.txt")

Tree = tree(datas, attributes)
print('Tree: ', Tree)
# Decided root node by comparing the gainRatio so far


