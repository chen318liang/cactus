#!/usr/bin/env python

"""Script to train pair-HMM of Cactus.
"""

import os
import random
from optparse import OptionParser
from jobTree.scriptTree.target import Target 
from jobTree.scriptTree.stack import Stack
from sonLib.bioio import setLoggingFromOptions, logger, system, popenCatch

class Hmm:
    def __init__(self, stateNumber=5):
        self.stateNumber = stateNumber
        self.transitions = [0.0] * stateNumber**2
        self.likelihood = 5.0
    
    def write(self, file):
        f = open(file, 'w')
        f.write(" ".join(map(str, self.transitions)) + (" %s\n" % self.likelihood))
        f.close()
     
    def addExpectationsFile(self, file):
        fH = open(file, 'r')
        l = map(float, fH.readline().split())
        assert len(l) == len(self.transitions)+1
        self.likelihood = l[-1]
        self.transitions = map(lambda x : sum(x), zip(self.transitions, l))
        assert len(self.transitions) == self.stateNumber**2
        fH.close()
    
    def normalise(self):
        """Normalises the EM probs.
        """
        for fromState in xrange(self.stateNumber):
            i = self.stateNumber * fromState
            j = sum(self.transitions[i:i+self.stateNumber])
            for toState in xrange(self.stateNumber):
                self.transitions[i + toState] = self.transitions[i + toState] / j

    def randomise(self):
        """Randomise the values in the HMM to small values.
        """
        self.transitions = map(lambda x : random.random(), range(self.stateNumber*self.stateNumber))
        self.normalise()

def expectationMaximisation(target, sequences, alignments, outputModel, options):
    #Iteratively run cactus realign to get expectations and load model.
    hmm = Hmm()
    if options.inputModel != None: #Read in the model
        target.logToMaster("Loading the model from the input file %s" % options.inputModel)
        hmm.addExpectationsFile(options.inputModel)
        hmm.normalise()
    elif options.randomStart: #Make random parameters
        target.logToMaster("Using random starting parameters")
        hmm.randomise()
    for iteration in xrange(options.iterations):
        #Temp file to store model
        modelsFile = os.path.join(target.getLocalTempDir(), "model.txt")
        hmm.write(modelsFile)
        #Run cactus_realign
        system("cat %s | cactus_realign --logLevel DEBUG %s --diagonalExpansion=10 --splitMatrixBiggerThanThis=3000 --loadHmm=%s --outputExpectations=%s" % (alignments, sequences, modelsFile, modelsFile))
        #Add to the expectations.
        hmm = Hmm()
        hmm.addExpectationsFile(modelsFile)
        hmm.normalise()
        #Do some logging
        target.logToMaster("After %i iteration got likelihood: %s" % (iteration, hmm.likelihood))
    hmm.write(outputModel)

def main():
    #Parse the inputs args/options
    parser = OptionParser(usage="usage: workingDir [options]", version="%prog 0.1")
    parser.add_option("--sequences", dest="sequences", help="Quoted list of fasta files containing sequences")
    parser.add_option("--alignments", dest="alignments", help="Cigar file ")
    parser.add_option("--inputModel", default=None, help="Input model")
    parser.add_option("--outputModel", default="hmm.txt", help="File to write the model in")
    parser.add_option("--iterations", default=10, help="Number of iterations of EM", type=int)
    parser.add_option("--randomStart", default=False, help="Iterate start model with small random values, else all values are equal", action="store_true")
    
    Stack.addJobTreeOptions(parser)
    options, args = parser.parse_args()
    setLoggingFromOptions(options)
    
    if len(args) != 0:
        raise RuntimeError("Expected no arguments, got %s arguments: %s" % (len(args), " ".join(args)))
    
    #Log the inputs
    logger.info("Got '%s' sequences, '%s' alignments file, '%s' output model and '%s' iterations of training" % (options.sequences, options.alignments, options.outputModel, options.iterations))

    #This line invokes jobTree  
    i = Stack(Target.makeTargetFn(expectationMaximisation, args=(options.sequences, options.alignments, options.outputModel, options))).startJobTree(options) 
    
    if i != 0:
        raise RuntimeError("Got failed jobs")

if __name__ == '__main__':
    from cactus.bar.cactus_expectationMaximisation import *
    main()