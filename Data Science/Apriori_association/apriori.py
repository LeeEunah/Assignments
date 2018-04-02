import sys      # import sys module for using command line arguments

# Read input file data
def readInputFile(fileName):
    transactions = []
    inputfile = open("./"+fileName, 'r')
    lines = inputfile.readlines()
    for line in lines:
        transaction = line[:-1].split('\t')
        transactions.append(transaction)
        
    inputfile.close()
    return transactions


def generateC1(data):
    C1 = []
    for transaction in data:
        for item in transaction:
            item_set = set()
            item_set.add(item)
            if item_set not in C1: # no duplicate element
                C1.append(item_set)

    return C1


# Count support count & Test minimum support
def scanDB(candidate, data, min_support):
    # support count
    item_candidate = {}
    for transaction in data:
        for item in candidate:
            if item.issubset(transaction):
                item = list(item)
                item.sort()
                item = tuple(item)
                if not item in item_candidate: # Count item
                    item_candidate[item] = 1
                else:
                   item_candidate[item] += 1


    # test minimum support
    Lk={}
    for key, value in item_candidate.items():
        support = (value / len(data)) * 100    # %
        if support >= min_support:          # Check minimum support
            Lk[key] = support

    return Lk


def generateCandidate(Lk, k):
    LkPlus = []
    # generate all candidate
    for i in range(len(Lk)):
        for j in range (i+1, len(Lk)):
            first = set(Lk[i])
            second = set(Lk[j])
            item_union = first | second
            if k == len(first | second) and item_union not in LkPlus:
                LkPlus.append(item_union)

    return LkPlus


def associationRule(Lk, item_support, k, file):
    if k != 2:
        file = open("./" + outputFile, 'a')
        
    Lk = list(Lk.keys())
    item_list = list(item_support.keys())
    support_list = list(item_support.values())
    
    for index in Lk:
        n = 0
        index = list(index)
        item = set(index)
        check = False

        if k == 2:
            index, check = subset(index, k, check)

        else: 
            while n < k-2:
                index, check = subset(index, k, check)
                n = n + 1
                
        for association in index:
            association = set(association)
            rest = item - association
            association_sort = list(tuple(association))
            association_sort.sort()
            association_sort = tuple(association_sort)
            union_sort = list(tuple(association | rest))
            union_sort.sort()
            union_sort = tuple(union_sort)

           # Calculate support and confidence
            support = round(item_support.get(union_sort), 2)
            confidence = round(((item_support.get(union_sort)) / (item_support.get(association_sort))) * 100, 2)

            # write to file
            file.write(str(association) + '\t' + str(rest) + '\t' + str(support) + '\t' + str(confidence) + '\n')
    file.close()
            


def subset(Lk_part, k, check):
    subset_item = []
    Lk_set = []

    if check == False:
        check = True
        for i in Lk_part:
            subset_set = set()
            subset_set.add(i)
            Lk_set.append(subset_set)
        

    else:
        Lk_set = Lk_part

    for i in Lk_set:
        for j in Lk_set:
            part_union = i | j
            if len(part_union) < k and part_union not in subset_item:
                subset_item.append(part_union)

    return subset_item, check
        

# main

# Check the command line arguments
if len(sys.argv) != 4:
    print('''Please fill in the command form.
Executable_file minimum_support inputfile outputfile''')
    exit()

    
min_support = int(sys.argv[1])
inputFile = sys.argv[2]
outputFile = sys.argv[3]

data = readInputFile(inputFile)
C1 = generateC1(data)
Lk = scanDB(C1, data, min_support)

item_support_data = Lk
Lk_before = Lk

file = open("./" + outputFile, 'w')
  
# repeat until end of the apriori
k = 2
while True:
    Ck = generateCandidate(list(Lk.keys()), k)
    
    if not Ck:
        break;
    
    Lk = scanDB(Ck, data, min_support)

    if not Lk:
        break;
    
    Lk_before = Lk
    item_support_data.update(Lk)

    if not Lk:
        associationRule(Lk_before, item_support_data, k, outputFile)
    else:
        associationRule(Lk, item_support_data, k, file)

    k = k + 1
    
