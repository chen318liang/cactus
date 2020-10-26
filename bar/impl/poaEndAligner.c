/**
 * This is designed as a drop-in replacement for the multiple aligner from pecan that gets used in the end aligner. 
 * The idea is that this will scale better for larger numbers of input samples, which seems to blow up the memory
 * in pecan.  
 *
 * Released under the MIT license, see LICENSE.txt
 */

#include "abpoa.h"
#include "poaEndAligner.h"
#include "../inc/poaEndAligner.h"

// char <--> uint8_t conversion copied over from abPOA example
// AaCcGgTtNn ==> 0,1,2,3,4
static unsigned char nst_nt4_table[256] = {
    4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4, 
    4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4, 
    4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4,  4, 5 /*'-'*/, 4, 4,
    4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4, 
    4, 0, 4, 1,  4, 4, 4, 2,  4, 4, 4, 4,  4, 4, 4, 4, 
    4, 4, 4, 4,  3, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4, 
    4, 0, 4, 1,  4, 4, 4, 2,  4, 4, 4, 4,  4, 4, 4, 4, 
    4, 4, 4, 4,  3, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4, 
    4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4, 
    4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4, 
    4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4, 
    4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4, 
    4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4, 
    4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4, 
    4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4, 
    4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4
};

/**
 * Convert a base in the POA alphabet to an ASCII base.
 */
static inline char toBase(uint8_t n) {
    return "ACGTN-"[n];
}

/**
 * Convert an ASCII base into the corresponding POA alphabet integer
 */
static inline uint8_t toByte(char c) {
    return nst_nt4_table[(int)c];
}

void msa_destruct(Msa *msa) {
    for(int64_t i=0; i<msa->seq_no; i++) {
        free(msa->seqs[i]);
        free(msa->msa_seq[i]);
    }
    free(msa->seqs);
    free(msa->msa_seq);
    free(msa->seq_lens);
    free(msa);
}

Msa *msa_make_partial_order_alignment(char **seqs, int *seq_lens, int64_t seq_no) {
    // Make Msa object
    Msa *msa = st_malloc(sizeof(Msa));
    msa->seq_no = seq_no;
    msa->seqs = seqs;
    msa->seq_lens = seq_lens;

    // collect sequence length, trasform ACGT to 0123
    uint8_t **bseqs = (uint8_t**)malloc(sizeof(uint8_t*) * msa->seq_no);

    for(int64_t i=0; i<msa->seq_no; i++) {
        bseqs[i] = (uint8_t *) malloc(sizeof(uint8_t) * seq_lens[i]);
        for (int64_t j = 0; j < seq_lens[i]; ++j) {
            // todo: support iupac characters?
            bseqs[i][j] = toByte(seqs[i][j]);
        }
    }

    // initialize variables
    abpoa_t *ab = abpoa_init();
    abpoa_para_t *abpt = abpoa_init_para();

    // todo: support including modifying abpoa params
    // alignment parameters
    // abpt->align_mode = 0; // 0:global alignment, 1:extension
    // abpt->match = 2;      // match score
    // abpt->mismatch = 4;   // mismatch penalty
    // abpt->gap_mode = ABPOA_CONVEX_GAP; // gap penalty mode
    // abpt->gap_open1 = 4;  // gap open penalty #1
    // abpt->gap_ext1 = 2;   // gap extension penalty #1
    // abpt->gap_open2 = 24; // gap open penalty #2
    // abpt->gap_ext2 = 1;   // gap extension penalty #2
                             // gap_penalty = min{gap_open1 + gap_len * gap_ext1, gap_open2 + gap_len * gap_ext2}
    // abpt->bw = 10;        // extra band used in adaptive banded DP
    // abpt->bf = 0.01; 
     
    // output options
    abpt->out_msa = 1; // generate Row-Column multiple sequence alignment(RC-MSA), set 0 to disable
    abpt->out_cons = 0; // generate consensus sequence, set 0 to disable

    abpoa_post_set_para(abpt);

    // perform abpoa-msa
    abpoa_msa(ab, abpt, msa->seq_no, NULL, msa->seq_lens, bseqs, NULL, NULL, NULL, NULL, NULL,
            &(msa->msa_seq), &(msa->column_no));

    // Clean up
    for (int64_t i = 0; i < seq_no; ++i) {
        free(bseqs[i]);
    }
    free(bseqs);

    abpoa_free(ab, abpt);
    abpoa_free_para(abpt);

    // in debug mode, cactus uses the dreaded -Wall -Werror combo.  This line is a hack to allow compilition with these flags
    if (false) SIMDMalloc(0, 0);

    return msa;
}

/**
 * Returns an array of floats, one for each corresponding column in the MSA. Each float
 * is the score of the column in the alignment.
 */
float *make_column_scores(Msa *msa) {
    float *column_scores = st_calloc(sizeof(float), msa->column_no);
    for(int64_t i=0; i<msa->column_no; i++) {
        for(int64_t j=0; j<msa->seq_no; j++) {
            if(toBase(msa->msa_seq[j][i]) != '-') {
                column_scores[i]++;
            }
        }
        if(column_scores[i] <= 1.0) { // Make score 0 for columns containing 1 aligned position
            column_scores[i] = 0.0;
        }
    }
    return column_scores;
}

/**
 * Fills in cu_column_scores with the cumulative sum of column scores, from left-to-right, of columns
 * containing a non-gap character in the given "row".
 */
void sum_column_scores(int64_t row, Msa *msa, float *column_scores, float *cu_column_scores) {
    float cu_score = 0.0; // The cumulative sum of column scores containing bases for the given row
    int64_t j=0; // The index in the DNA string for the given row
    for(int64_t i=0; i<msa->column_no; i++) {
        if(toBase(msa->msa_seq[row][i]) != '-') {
            cu_score += column_scores[i];
            cu_column_scores[j++] = cu_score;
        }
    }
    assert(msa->seq_lens[row] == j); // We should cover all the bases in the DNA sequence
}

/**
 * Removes the suffix of the given row from the MSA and updates the column scores. suffix_start is the beginning
 * suffix to remove.
 */
void trim_msa_suffix(Msa *msa, float *column_scores, int64_t row, int64_t suffix_start) {
    int64_t seq_index = 0;
    for(int64_t i=0; i<msa->column_no; i++) {
        if(toBase(msa->msa_seq[row][i]) != '-') {
            if(seq_index++ >= suffix_start) {
                msa->msa_seq[row][i] = toByte('-');
                column_scores[i] = column_scores[i]-1 > 0 ? column_scores[i]-1 : 0;
            }
        }
    }
}

/**
 * Used to make two MSAs consistent with each other for a shared sequence
 */
void trim(int64_t row1, Msa *msa1, float *column_scores1,
          int64_t row2, Msa *msa2, float *column_scores2) {
    int64_t seq_len = msa1->seq_lens[row1]; // The length of the shared sequence
    assert(seq_len == msa2->seq_lens[row2]);

    // Get the cumulative cut scores for the columns containing the shared sequence
    float cu_column_scores1[msa1->column_no];
    float cu_column_scores2[msa2->column_no];
    sum_column_scores(row1, msa1, column_scores1, cu_column_scores1);
    sum_column_scores(row2, msa2, column_scores2, cu_column_scores2);

    float max_cut_score = cu_column_scores2[seq_len-1]; // The score if we cut all of msa1 and keep all of msa2
    int64_t max_cut_point = 0; // the length of the prefix of msa1 to keep
    for(int64_t i=0; i<seq_len-1; i++) {
        assert(seq_len-i-2 >= 0); // Sanity check
        float cut_score = cu_column_scores1[i] + cu_column_scores2[seq_len-i-2]; // The score if we keep prefix up to
        // and including column 1 of MSA1, and the prefix of msa2 up to and including column seq_len-i-2
        if(cut_score > max_cut_score) {
            max_cut_point = i+1;
            max_cut_score = cut_score;
        }
    }
    if(cu_column_scores1[seq_len-1] > max_cut_score) { // The score if we cut all of msa2 and keep all of msa1
        max_cut_point = seq_len;
        max_cut_score = cu_column_scores1[seq_len-1];
    }

    // Now trim back the two MSAs
    trim_msa_suffix(msa1, column_scores1, row1, max_cut_point);
    trim_msa_suffix(msa2, column_scores2, row2, seq_len-max_cut_point);
}

Msa **makeConsistentPartialOrderAlignments(int64_t end_no, int64_t *end_lengths, char ***end_strings,
        int **end_string_lengths, int64_t **right_end_indexes, int64_t **right_end_row_indexes) {
    // Calculate the initial, potentially inconsistent msas and column scores for each msa
    float *column_scores[end_no];
    Msa **msas = st_malloc(sizeof(Msa *) * end_no);
    for(int64_t i=0; i<end_no; i++) {
        msas[i] = msa_make_partial_order_alignment(end_strings[i], end_string_lengths[i], end_lengths[i]);
        column_scores[i] = make_column_scores(msas[i]);
    }

    // Make the msas consistent with one another
    for(int64_t i=0; i<end_no; i++) { // For each end
        Msa *msa = msas[i];
        for(int64_t j=0; j<msa->seq_no; j++) { //  For each string incident to the ith end
            int64_t right_end_index = right_end_indexes[i][j]; // Find the other end it is incident with
            int64_t right_end_row_index = right_end_row_indexes[i][j]; // And the index of its reverse complement

            // If it hasn't already been trimmed
            if(right_end_index > i || (right_end_index == i /* self loop */ && right_end_row_index > j)) {
                trim(j, msa, column_scores[i],
                        right_end_row_index, msas[right_end_index], column_scores[right_end_index]);
            }
        }
    }

    return msas;
}

/**
 * The follow code is for dealing with the cactus API
 */


void alignmentBlock_destruct(AlignmentBlock *alignmentBlock) {
    AlignmentBlock *a;
    while(alignmentBlock->next != NULL) {
        a = alignmentBlock;
        alignmentBlock = alignmentBlock->next;
        free(a);
    }
}

/**
 * Get the string connecting two ends for the given cap
 */
static char *get_adjacency_string(Cap *cap, int *length) {
    assert(!cap_getSide(cap));
    Sequence *sequence = cap_getSequence(cap);
    assert(sequence != NULL);
    Cap *cap2 = cap_getAdjacency(cap);
    assert(cap2 != NULL);
    assert(cap_getSide(cap2));
    if (cap_getStrand(cap)) {
        *length = cap_getCoordinate(cap2) - cap_getCoordinate(cap) - 1;
        assert(*length >= 0);
        return sequence_getString(sequence, cap_getCoordinate(cap) + 1, *length, 1);
    } else {
        *length = cap_getCoordinate(cap) - cap_getCoordinate(cap2) - 1;
        assert(*length >= 0);
        return sequence_getString(sequence, cap_getCoordinate(cap2) + 1, *length, 0);
    }
}

/**
 * Gets the length and sequences present in the next maximal gapless alignment block.
 * @param msa The msa to scan
 * @param start The start of the gapless block
 * @param rows_in_block A boolean array of which sequences are present in the block
 * @param sequences_in_block The number of in the block
 * @return
 */
int64_t get_next_maximal_block_dimensions(Msa *msa, int64_t start, bool *rows_in_block, int64_t *sequences_in_block) {
    // Calculate which sequences are in the block
    *sequences_in_block = 0;
    for(int64_t i=0; i<msa->seq_no; i++) {
        rows_in_block[i] = toBase(msa->msa_seq[i][start]) != '-';
        *sequences_in_block += 1;
    }

    // Calculate the maximal block length by looking at successive columns of the MSA and
    // checking they have the same set of sequences present as in the first block
    int64_t end = start;
    while(++end < msa->column_no) {
        for(int64_t i=0; i<msa->seq_no; i++) {
            bool p = toBase(msa->msa_seq[i][end]) == '-';
            if(p != rows_in_block[i]) {
                return end;
            }
        }
    }
    return end;
}

/**
 * Make an alignment block for the given interval and sequences
 * @param seq_no The number of sequences in the MSA
 * @param start The start, inclusive, of the block
 * @param length The of the block
 * @param rows_in_block An array specifying which sequences are in the block
 * @param seq_indexes The start coordinates of the sequences in the block
 * @param row_indexes_to_caps The Caps corresponding to the sequences in the block
 * @return The new alignment block
 */
AlignmentBlock *make_alignment_block(int64_t seq_no, int64_t start, int64_t length, bool *rows_in_block,
                                     int64_t *seq_indexes, Cap **row_indexes_to_caps) {
    AlignmentBlock *pB = NULL, *block = NULL;
    for(int64_t i=0; i<seq_no; i++) { // For each row
        if(rows_in_block[i]) { // If the row is in the block
            // Make an alignment block
            AlignmentBlock *b = st_calloc(1, sizeof(AlignmentBlock));
            Cap *cap = row_indexes_to_caps[i];
            assert(!cap_getSide(cap));
            b->subsequenceIdentifier = cap_getName(cap);
            b->strand = cap_getStrand(cap);
            b->length = length;
            // Calculate the sequence coordinate using Cactus coordinates
            if(b->strand) {
                b->position = seq_indexes[i] + cap_getCoordinate(cap) + 1;
            }
            else {
                b->position = cap_getCoordinate(cap) - 1 - seq_indexes[i];
            }

            // If this is not the first sequence in the block link to the previous sequence in the block
            if (pB != NULL) {
                pB->next = b;
                pB = b;
            } else { // Otherwise this is the first sequence in the block
                pB = b;
                block = b;
            }
        }
    }
    return block;
}

/**
 * Converts an Msa into a list of AlignmentBlocks.
 * @param msa The msa to convert
 * @param row_indexes_to_caps The Caps for each sequence in the MSA
 * @param alignment_blocks The list to add the alignment blocks to
 */
void create_alignment_blocks(Msa *msa, Cap **row_indexes_to_caps, stList *alignment_blocks) {
    int64_t i=0; // The left most index of the current block
    int64_t j=0; // The right most index of the current block
    bool rows_in_block[msa->seq_no]; // An array of bools used to indicate which sequences are present in a block
    int64_t seq_indexes[msa->seq_no]; // The start offsets of the current block
    int64_t sequences_in_block; // The number of sequences in the block

    // Walk through successive gapless blocks
    while((j += get_next_maximal_block_dimensions(msa, j,
                                                  rows_in_block, &sequences_in_block)) < msa->column_no) {
        assert(j > i);

        // Make the next alignment block
        if(sequences_in_block > 1) { // Only make a block if it contains two or more sequences
            stList_append(alignment_blocks, make_alignment_block(msa->seq_no, i, j - i, rows_in_block,
                                                                 seq_indexes, row_indexes_to_caps));
        }

        // Update the offsets in the sequences in the block, regardless of if we actually
        // created the block
        for(int64_t k=0; k<msa->seq_no; k++) {
            if(rows_in_block[k]) {
                seq_indexes[k] += j - i;
            }
        }

        i = j;
    }
}

stList *makeFlowerAlignmentPOA(Flower *flower, bool pruneOutStubAlignments) {
    // Arrays of ends and connecting the strings necessary to build the POA alignment
    int64_t end_no = flower_getEndNumber(flower); // The number of ends
    int64_t end_lengths[end_no]; // The number of strings incident with each end
    char **end_strings[end_no]; // The actual strings connecting the ends
    int *end_string_lengths[end_no]; // Length of the strings connecting the ends
    int64_t *right_end_indexes[end_no];  // For each string the index of the right end that it is connecting
    int64_t *right_end_row_indexes[end_no]; // For each string the index of the row of its reverse complement

    // Data structures to translate between caps and sequences in above end arrays
    Cap **indices_to_caps[end_no]; // For each string the corresponding Cap
    stHash *caps_to_indices = stHash_construct2(NULL, free); // A hash of caps to their end and row indices

    // Fill out the end information for building the POA alignments arrays
    End *end;
    Flower_EndIterator *endIterator = flower_getEndIterator(flower);
    int64_t i=0; // Index of the end
    while ((end = flower_getNextEnd(endIterator)) != NULL) {
        // Initialize the various arrays for the end
        end_lengths[i] = end_getInstanceNumber(end); // The number of strings incident with the end
        end_strings[i] = st_malloc(sizeof(char *)*end_lengths[i]);
        end_string_lengths[i] = st_malloc(sizeof(int)*end_lengths[i]);
        right_end_indexes[i] = st_malloc(sizeof(int64_t)*end_lengths[i]);
        right_end_row_indexes[i] = st_malloc(sizeof(int64_t)*end_lengths[i]);
        indices_to_caps[i] = st_malloc(sizeof(Cap *)*end_lengths[i]);

        // Now get each string incident with the end
        Cap *cap;
        End_InstanceIterator *capIterator = end_getInstanceIterator(end);
        int64_t j=0; // Index of the cap in the end's arrays
        while ((cap = end_getNext(capIterator)) != NULL) {
            assert(j < end_lengths[i]);
            // Ensure we have the cap in the correct orientation
            if (cap_getSide(cap)) {
                cap = cap_getReverse(cap);
            }
            // Get the string and its length
            end_strings[i][j] = get_adjacency_string(cap, &(end_string_lengths[i][j]));

            // Populate the caps to end/row indices, and vice versa, data structures
            indices_to_caps[i][j] = cap;
            stHash_insert(caps_to_indices, cap, stIntTuple_construct2(i, j));

            j++;
        }
        end_destructInstanceIterator(capIterator);
        assert(end_lengths[i] == j);
        i++;
    }
    flower_destructEndIterator(endIterator);

    // Fill out the end / row indices for each cap
    endIterator = flower_getEndIterator(flower);
    i=0;
    while ((end = flower_getNextEnd(endIterator)) != NULL) {
        Cap *cap;
        End_InstanceIterator *capIterator = end_getInstanceIterator(end);
        int64_t j=0;
        while ((cap = end_getNext(capIterator)) != NULL) {
            if (cap_getSide(cap)) {
                cap = cap_getReverse(cap);
            }
            Cap *cap2 = cap_getAdjacency(cap);
            assert(cap2 != NULL);
            cap2 = cap_getReverse(cap2);
            assert(!cap_getSide(cap));
            assert(!cap_getSide(cap2));
            stIntTuple *k = stHash_search(caps_to_indices, cap2);
            assert(k != NULL);

            right_end_indexes[i][j] = stIntTuple_get(k, 0);
            right_end_row_indexes[i][j] = stIntTuple_get(k, 1);

            j++;
        }
        end_destructInstanceIterator(capIterator);
        assert(end_lengths[i] == j);
        i++;
    }
    flower_destructEndIterator(endIterator);

    // Now make the consistent MSAs
    Msa **msas = makeConsistentPartialOrderAlignments(end_no, end_lengths, end_strings, end_string_lengths,
                                                      right_end_indexes, right_end_row_indexes);

    // TODO: stub-alignments?

    //Now convert to set of alignment blocks
    stList *alignment_blocks = stList_construct3(0, (void (*)(void *))alignmentBlock_destruct);
    for(int64_t i=0; i<end_no; i++) {
        create_alignment_blocks(msas[i], indices_to_caps[i], alignment_blocks);
    }

    // Cleanup
    for(int64_t i=0; i<end_no; i++) {
        free(msas[i]);
        free(right_end_indexes[i]);
        free(right_end_row_indexes[i]);
        free(indices_to_caps[i]);
    }
    stHash_destruct(caps_to_indices);

    return alignment_blocks;
}
