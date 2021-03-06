// Class to describe an RNA expression matrix.

#ifndef CZI_EXPRESSION_MATRIX2_EXPRESSION_MATRIX_HPP
#define CZI_EXPRESSION_MATRIX2_EXPRESSION_MATRIX_HPP

// CZI.
#include "Cell.hpp"
#include "CellGraph.hpp"
#include "CellSets.hpp"
#include "GeneSet.hpp"
#include "HttpServer.hpp"
#include "Ids.hpp"
#include "MemoryMappedVector.hpp"
#include "MemoryMappedVectorOfLists.hpp"
#include "MemoryMappedVectorOfVectors.hpp"
#include "MemoryMappedStringTable.hpp"
#include "NormalizationMethod.hpp"

// Standard library.
#include <limits>
#include "map.hpp"
#include "memory.hpp"
#include "string.hpp"
#include "utility.hpp"

namespace ChanZuckerberg {
    namespace ExpressionMatrix2 {

        class BitSet;
        class CellGraph;
        class CellGraphInformation;
        class ClusterGraph;
        class ClusterGraphCreationParameters;
        class ExpressionMatrix;
        class ExpressionMatrixCreationParameters;
        class ExpressionMatrixSubset;
        class GeneGraph;
        class Lsh;
        class ServerParameters;
        class SimilarPairs;
        class SignatureGraph;

    }
}

// Forward declarations necessary for functions that return a numpy array,
// or that take numpy arrays as arguments.
namespace pybind11 {
    class array;
    class buffer;
}



// Class used to store various parameters that control the initial creation of
// the ExpressionMatrix.
class ChanZuckerberg::ExpressionMatrix2::ExpressionMatrixCreationParameters {
public:

    // The following parameters control the capacity of various hash tables
    // use to store strings.
    // These capacities are hard limits: after the capacity is reached,
    // inserting a new element triggers an endless loop
    // (because we use open addressing hash tables without rehashing and without checks).
    // For good performance of these hash tables, these capacities
    // should equal at least twice the actual expected number of strings
    // of each type that will be stored.
    uint64_t geneCapacity = 1<<18;              // Controls the maximum number of genes.
    uint64_t cellCapacity = 1<<24;              // Controls the maximum number of cells.
    uint64_t cellMetaDataNameCapacity = 1<<16;  // Controls the maximum number of distinct cell meta data name strings.
    uint64_t cellMetaDataValueCapacity = 1<<28; // Controls the maximum number of distinct cell meta data value strings.
    uint64_t geneMetaDataNameCapacity = 1<<16;  // Controls the maximum number of distinct gene meta data name strings.
    uint64_t geneMetaDataValueCapacity = 1<<20; // Controls the maximum number of distinct gene meta data value strings.

    ExpressionMatrixCreationParameters(
        uint64_t geneCapacity,
        uint64_t cellCapacity,
        uint64_t cellMetaDataNameCapacity,
        uint64_t cellMetaDataValueCapacity,
        uint64_t geneMetaDataNameCapacity,
        uint64_t geneMetaDataValueCapacity
        );
};



// Class used to store information about a cell graph.
class ChanZuckerberg::ExpressionMatrix2::CellGraphInformation {
public:
    string cellSetName;
    string similarPairsName;
    double similarityThreshold;
    size_t maxConnectivity;
    size_t vertexCount;
    size_t edgeCount;
    size_t isolatedRemovedVertexCount;     // The number of isolated vertices that were removed.
    CellGraphInformation() {}
};



// Class used to specify parameters when starting the http server.
class ChanZuckerberg::ExpressionMatrix2::ServerParameters {
public:
    uint16_t port = 17100;  // The port number to listen to.
    string docDirectory;    // The directory containing the documentation (optional).
    ServerParameters() {}
    ServerParameters(uint16_t port, string docDirectory);
};



class ChanZuckerberg::ExpressionMatrix2::ExpressionMatrix : public HttpServer {
public:


    // Construct a new expression matrix. All binary data for the new expression matrix
    // will be stored in the specified directory. If the directory does not exist,
    // it will be created. If the directory already exists, any previous
    // expression matrix stored in the directory will be overwritten by the new one.
    ExpressionMatrix(const string& directoryName, const ExpressionMatrixCreationParameters&);
    ExpressionMatrix(
        const string& directoryName,
        uint64_t geneCapacity,
        uint64_t cellCapacity,
        uint64_t cellMetaDataNameCapacity,
        uint64_t cellMetaDataValueCapacity,
        uint64_t geneMetaDataNameCapacity,
        uint64_t geneMetaDataValueCapacity
    );

    // Access a previously created expression matrix stored in the specified directory.
    ExpressionMatrix(const string& directoryName, bool allowReadOnly);

    // Add a gene.
    // Returns true if the gene was added, false if it was already present.
    bool addGene(const string& geneName);

    // Add a cell to the expression matrix.
    // The meta data is passed as a vector of names and values, which are all strings.
    // The cell name should be entered as meta data "CellName".
    // The expression counts for each gene are passed as a vector of pairs
    // (gene names, count).
    // Returns the id assigned to this cell.
    // This changes the metaData vector so the CellName entry is the first entry.
    // It also changes the expression counts - it sorts them by decreasing count.
    CellId addCell(
        const vector< pair<string, string> >& metaData,
        const vector< pair<string, float> >& expressionCounts
        );

    // Version of addCell that takes JSON as input.
    // The expected JSON can be constructed using Python code modeled from the following:
    // import json
    // cell = {'metaData': {'CellName': 'abc', 'key1': 'value1'}, 'expressionCounts': {'gene1': 10,'gene2': 20}}
    // expressionMatrix.addCellFromJson(json.dumps(jSonString))
    // Note that the cellName metaData entry is required.
    CellId addCellFromJson(const string& jsonString);



    /*******************************************************************************

    Add cells from data in files with fields separated by commas or by other separators.
    A fields can contain separators, as long as the entire field is quoted.

    This requires two input files, one for expression counts and one for cell meta data.
    The separators for each file are specified as arguments to this function.

    The expression counts file has a row for each gene and a column for each cell.
    In addition, it has an additional header row, before all other rows, containing cell names,
    and an additional column, before all other columns, containing gene names.
    The entry in the first column of the first row is ignored but must be present (can be empty).

    The meta data file has a row for each cell and a colun for each meta data field name.
    In addition, it has an additional header row, before all other rows, containing meta data names,
    and an additional column, before all other columns, containing cell names.
    Again, the entry in the first column of the first row is ignored but must be present (can be empty).

    The cell names in the first row of the expression count file and in the first column of the meta data file
    don't have to be in the same order.

    If a cell name is present in only one of the files, that cell is ignored.

    An example of the two files follow:

    Expression counts file:
    Dontcare,Cell1,Cell2,Cell3
    Gene1,10,20,30
    Gene2,30,40,50

    Meta data file:
    Dontcare,Name1,Name2
    Cell1,abc,def
    Cell2,123,456
    Cell3,xyz,uv

    *******************************************************************************/
    void addCells(
        const string& expressionCountsFileName,
        const string& expressionCountsFileSeparators,
        const string& metaDataFileName,
        const string& metaDataFileSeparators,
        const vector< pair<string, string> >& additionalCellMetaData // Added to all cells.
        );
    // Old version that needs much more memory.
    void addCellsOld1(
        const string& expressionCountsFileName,
        const string& expressionCountsFileSeparators,
        const string& metaDataFileName,
        const string& metaDataFileSeparators
        );



    /*******************************************************************************

     Add cells from an hdf5 file in the format typically created by the 10X Genomics
     pipeline.

     See here for an example of a typical hdf5 file that we want to process:
     http://cf.10xgenomics.com/samples/cell-exp/1.3.0/t_3k_4k_aggregate/t_3k_4k_aggregate_filtered_gene_bc_matrices_h5.h5

     You can use command hdf5ls (from package hdf5-tools) to list the contents of the file:

     h5ls -r t_3k_4k_aggregate_filtered_gene_bc_matrices_h5.h5
     /                        Group
     /GRCh38                  Group
     /GRCh38/barcodes         Dataset {8083}
     /GRCh38/data             Dataset {8863417/Inf}
     /GRCh38/gene_names       Dataset {33694}
     /GRCh38/genes            Dataset {33694}
     /GRCh38/indices          Dataset {8863417/Inf}
     /GRCh38/indptr           Dataset {8084/8192}
     /GRCh38/shape            Dataset {2/16384}

     Data sets data, indices, and indptr contain the expression matrix in sparse format, by cell.

     The gene names are in data set gene_names. Data set genes is not used. It contains alternative
     gene ids.

     Cell names are generated using the barcodes dataset, prefixing each barcode
     with the given cellNamePrefix.

     Data set shape is also not used.

     Only cells for which the total expression count equals or exceeded
     the given threshold are added.

     *******************************************************************************/
#ifndef CZI_EXPRESSION_MATRIX2_SKIP_HDF5
    void addCellsFromHdf5(
        const string& fileName,
        const string& cellNamePrefix,
        const vector< pair<string, string> > cellMetaData,  // Added to all cells.
        double totalExpressionCountThreshold);
#endif


    // Add cells from files created by the BioHub pipeline.
    // See top of ExpressionMatrixBioHub.cpp for a detailed description
    // of the expected formats.

    // July 2017, Illumina data.
    void addCellsFromBioHub1(
        const string& expressionCountsFileName, // The name of the csv file containing expression counts.
        size_t initialMetaDataCount,            // The number of initial columns containing meta data.
        size_t finalMetaDataCount,              // The number of final columns containing meta data.
        const string& plateMetaDataFileName     // The name of the file containing per-plate meta data.
        );
    void getPlateMetaDataFromBioHub(
        const string& plateName,
        const string&plateMetaDataFileName,
        vector<pair<string, string> >& plateMetaData);

    // September 2017, 10X Genomics data.
    // See top of ExpressionMatrixBioHub.cpp for details.
#ifndef CZI_EXPRESSION_MATRIX2_SKIP_HDF5
    void addCellsFromBioHub2(
        const string& platesFileName,    // The name of the file containing per-plate meta data.
        double totalExpressionCountThreshold
        );
#endif

    // November 2017, Illumina data.
    // See top of ExpressionMatrixBioHub.cpp for details.
    void addCellsFromBioHub3(
        const string& expressionCountsFileName,         // The name of the csv file containing expression counts.
        const string& expressionCountsFileSeparators,
        const vector<pair<string, string> >& plateMetaData  // Meta data that will be added to all cells.
        );




    // Add cell meta data contained in a csv file, one line per cell.
    // This can be used to read cell meta data in the BioHub pipeline
    // stored in the .log-by-cell.csv files.
    void addCellMetaData(const string& cellMetaDataName);



    // Return the number of genes.
    GeneId geneCount() const
    {
        return GeneId(geneNames.size());
    }

    // Return the number of cells.
    CellId cellCount() const
    {
        return CellId(cellMetaData.size());
    }

    // Return the value of a specified meta data field for a given cell.
    // Returns an empty string if the cell does not have the specified meta data field.
    string getCellMetaData(CellId, const string& name) const;
    string getCellMetaData(CellId, StringId) const;

    // Return a vector containing all of the meta data (Name, Value) pairs
    // for a given cell.
    vector< pair<string, string> > getCellMetaData(CellId) const;

    // Return a vector containing vectors with all of the meta data (Name, Value) pairs
    // for a given set of cells.
    vector< vector< pair<string, string> > > getCellMetaData(const vector<CellId>&) const;

    // Set a meta data (name, value) pair for a given cell.
    // If the name already exists for that cell, the value is replaced.
    void setCellMetaData(CellId, const string& name, const string& value);
    void setCellMetaData(CellId, StringId nameId, const string& value);
    void setCellMetaData(CellId, StringId nameId, StringId valueId);

    // Remove a meta data field for all cells of a given cell set.
    void removeCellMetaData(const string& cellSetName, const string& metaDataName);

    // Compute a sorted histogram of a given meta data field.
    void histogramMetaData(
        const CellSet& cellSet,
        StringId metaDataNameId,
        vector< pair<string, size_t> >& sortedHistogram) const;

    // Compute Rand Index and Adjusted Rand Index for two
    // meta data fields.
    pair<double, double> computeMetaDataRandIndex(
        const string& cellSetName,
        const string& metaDataName0,
        const string& metaDataName1);

    // Compute the similarity between two cells given their CellId.
    // The similarity is the correlation coefficient of their
    // expression counts.
    double computeCellSimilarity(CellId, CellId) const;
    double computeCellSimilarity(const string& geneSetName, CellId, CellId) const;
    double computeCellSimilarity(const GeneSet&, CellId, CellId) const;

    // Compute the average expression vector for a given set of cells.
    // The last parameter controls the normalization used for the expression counts
    // for averaging:
    // 0: no normalization (raw read counts).
    // 1: L1 normalization (fractional read counts).
    // 2: L2 normalization.
    void computeAverageExpression(
        const GeneSet& geneSet,
        const vector<CellId> cells,
        vector<double>& averageExpression,
        NormalizationMethod normalizationMethod) const;



    // Gene set creation and manipulation.

    // Create a gene set using an information content criterion.
    // All genes with information content greater than the specified value
    // are added to the set.
    // Information content is measured in bits. The specified input
    // gene set and cell set are used to compute information content
    // with the specified normalization method.
    void createGeneSetUsingInformationContent(
        const string& existingGeneSetName,
        const string& cellSetName,
        NormalizationMethod normalizationMethod,
        double geneInformationContentThreshold,
        const string& newGeneSetName);
    void createGeneSetUsingInformationContent(
        const GeneSet& existingGeneSet,
        const CellSet& cellSet,
        NormalizationMethod normalizationMethod,
        double geneInformationContentThreshold,
        GeneSet& newGeneSet) const;

    // Create a new gene set as the intersection or union of two or more existing gene sets.
    // The input gene sets are specified comma separated in the first argument.
    // Return true if successful, false if one of the input gene sets does not exist
    // or the output gene set already exists.
    bool createGeneSetIntersection(const string& inputSets, const string& outputSet);
    bool createGeneSetUnion(const string& inputSets, const string& outputSet);
    bool createGeneSetIntersectionOrUnion(const string& inputSets, const string& outputSet, bool doUnion);

    // Create a new gene set as the difference between two existing gene sets.
    // Return true if successful, false if one of the input gene sets does not exist
    // or the output gene set already exists.
    bool createGeneSetDifference(const string& inputSet0, const string& inputSet1, const string& outputSet);



    // Find similar cell pairs by looping over all pairs,
    // taking into account only genes in the specified gene set.
    // This is O(N**2) slow because it loops over cell pairs.
    // It typically takes of the order of 20 microseconds per pair.

    void findSimilarPairs0(
        const string& geneSetName,  // The name of the gene set to be used.
        const string& cellSetName,  // The name of the cell set to be used.
        const string& name,         // The name of the SimilarPairs object to be created.
        size_t k,                   // The maximum number of similar pairs to be stored for each cell.
        double similarityThreshold
        );
    void findSimilarPairs0(
        ostream& out,
        const string& geneSetName,  // The name of the gene set to be used.
        const string& cellSetName,  // The name of the cell set to be used.
        const string& name,         // The name of the SimilarPairs object to be created.
        size_t k,                   // The maximum number of similar pairs to be stored for each cell.
        double similarityThreshold
        );



    // Find similar cell pairs by looping over all pairs
    // and using an LSH approximation to compute the similarity between two cells.
    // This is also O(N**2) slow because it loops over cell pairs,
    // but much faster than findSimilarPairs0.
    // It typically takes of the order of 15 nanoseconds per pair
    // (for lshCount=1024, which guarantees rms error 0.05 or better
    // on computed similarities).
    void findSimilarPairs4(
        const string& geneSetName,      // The name of the gene set to be used.
        const string& cellSetName,      // The name of the cell set to be used.
        const string& name,             // The name of the SimilarPairs object to be created.
        size_t k,                       // The maximum number of similar pairs to be stored for each cell.
        double similarityThreshold,     // The minimum similarity for a pair to be stored.
        size_t lshCount,                // The number of LSH vectors to use.
        unsigned int seed               // The seed used to generate the LSH vectors.
        );
    void findSimilarPairs4(
        ostream&,
        const string& geneSetName,      // The name of the gene set to be used.
        const string& cellSetName,      // The name of the cell set to be used.
        const string& name,             // The name of the SimilarPairs object to be created.
        size_t k,                       // The maximum number of similar pairs to be stored for each cell.
        double similarityThreshold,     // The minimum similarity for a pair to be stored.
        size_t lshCount,                // The number of LSH vectors to use.
        unsigned int seed               // The seed used to generate the LSH vectors.
        );
#if CZI_EXPRESSION_MATRIX2_BUILD_FOR_GPU
    void findSimilarPairs4Gpu(
        const string& geneSetName,      // The name of the gene set to be used.
        const string& cellSetName,      // The name of the cell set to be used.
        const string& lshName,          // The name of the Lsh object to be used.
        const string& similarPairsName, // The name of the SimilarPairs object to be created.
        size_t k,                       // The maximum number of similar pairs to be stored for each cell.
        double similarityThreshold,     // The minimum similarity for a pair to be stored.
        size_t lshCount,                // The number of LSH vectors to use.
        unsigned int seed,              // The seed used to generate the LSH vectors.
        unsigned int kernel,            // The GPU kernel (algorithm) to use.
        CellId blockSize                // The number of cells processed by each kernel instance.
        );
    void findSimilarPairs4GpuKernel0(
        size_t k,                       // The maximum number of similar pairs to be stored for each cell.
        double similarityThreshold,     // The minimum similarity for a pair to be stored.
        Lsh& lsh,
        SimilarPairs& similarPairs);
    void findSimilarPairs4GpuKernel1(
        size_t k,                       // The maximum number of similar pairs to be stored for each cell.
        double similarityThreshold,     // The minimum similarity for a pair to be stored.
        Lsh& lsh,
        SimilarPairs& similarPairs,
        CellId blockSize);
    void findSimilarPairs4GpuKernel2(
        size_t k,                       // The maximum number of similar pairs to be stored for each cell.
        double similarityThreshold,     // The minimum similarity for a pair to be stored.
        Lsh& lsh,
        SimilarPairs& similarPairs,
        CellId blockSize);
#endif

    // Find similar cell pairs using the full LSH algorithm, without looping over all pairs.
    // This is prototype code.
    void findSimilarPairs5(
        const string& geneSetName,      // The name of the gene set to be used.
        const string& cellSetName,      // The name of the cell set to be used.
        const string& lshName,          // The name of the Lsh object to be used.
        const string& similarPairsName, // The name of the SimilarPairs object to be created.
        size_t k,                       // The maximum number of similar pairs to be stored for each cell.
        double similarityThreshold,     // The minimum similarity for a pair to be stored.
        size_t lshSliceLength,          // The number of bits in each LSH signature slice, or 0 for automatic selection.
        size_t bucketOverflow           // If not zero, ignore buckets larger than this.
        );

    // Find similar cell pairs using LSH and the Charikar algorithm.
    // See M. Charikar, "Similarity Estimation Techniques from Rounding Algorithms", 2002,
    // section "5. Approximate Nearest neighbor Search in Hamming Space.".
    // The Charikar algorithm is for approximate nearest neighbor, but with appropriate
    // choices of the algorithm parameters permutationCount and searchCount
    // can be used for approximate k nearest neighbors.
    // In the Charikar paper, permutationCount is N and searchCount is 2N.
    // To reduce memory requirements, we don't store all bits all bits of
    // the permuted signatures - only the most significant permutedBitCount.
    // In practice it is best to set this to 64, so the permuted signatured
    // use only one 64-bit word each.
    void findSimilarPairs6(
        const string& geneSetName,      // The name of the gene set to be used.
        const string& cellSetName,      // The name of the cell set to be used.
        const string& lshName,          // The name of the Lsh object to be used.
        const string& similarPairsName, // The name of the SimilarPairs object to be created.
        size_t k,                       // The maximum number of similar pairs to be stored for each cell.
        double similarityThreshold,     // The minimum similarity for a pair to be stored.
        size_t permutationCount,        // The number of bit permutations for the Charikar algorithm.
        size_t searchCount,             // The number of cells checked for each cell, in the Charikar algorithm.
        size_t permutedBitCount,        // The number of most significant bits stored for each permuted signature.
        int seed                        // The seed used to randomly generate the bit permutations.
        );

    // Find similar cell pairs using the full LSH algorithm, without looping over all pairs.
    // Like findSimilarPairs5, but using variable lsh slice length.
    // This is prototype code.
    void findSimilarPairs7(
        const string& geneSetName,      // The name of the gene set to be used.
        const string& cellSetName,      // The name of the cell set to be used.
        const string& lshName,          // The name of the Lsh object to be used.
        const string& similarPairsName, // The name of the SimilarPairs object to be created.
        size_t k,                       // The maximum number of similar pairs to be stored for each cell.
        double similarityThreshold,     // The minimum similarity for a pair to be stored.
        const vector<int>& lshSliceLengths, // The number of bits in each LSH signature slice, in decreasing order.
        CellId maxCheck,                // Maximum number of cells to consider for each cell.
        size_t log2BucketCount
    );
    void findSimilarPairs7AssignCellsToBuckets(
        Lsh&,
        const vector<int>& lshSliceLengths,                     // The number of signature slice bits, in decreasing order.
        vector< vector< vector< size_t > > >& sliceBits3,       // The bits of each slice.
        vector< vector< vector< vector<CellId> > > >& table4,   // The cells in each bucket.
        size_t log2BucketCount
        );
#if CZI_EXPRESSION_MATRIX2_BUILD_FOR_GPU
    // GPU version. See Lsh.cl for details.
    void findSimilarPairs7Gpu(
        const string& geneSetName,          // The name of the gene set to be used.
        const string& cellSetName,          // The name of the cell set to be used.
        const string& lshName,              // The name of the Lsh object to be used.
        const string& similarPairsName,     // The name of the SimilarPairs object to be created.
        size_t k,                           // The maximum number of similar pairs to be stored for each cell.
        double similarityThreshold,         // The minimum similarity for a pair to be stored.
        const vector<int>& lshSliceLengths, // The number of bits in each LSH signature slice, in decreasing order.
        CellId maxCheck,                    // Maximum number of candidate neighbors to consider for each cell.
        size_t log2BucketCount,
        CellId blockSize                    // The number of cells processed by each kernel instance on the GPU.
        );
    void findSimilarPairs7GpuKernel3(
        size_t k,                       // The maximum number of similar pairs to be stored for each cell.
        double similarityThreshold,     // The minimum similarity for a pair to be stored.
        Lsh& lsh,
        SimilarPairs& similarPairs,
        const vector<int>& lshSliceLengths, // The number of bits in each LSH signature slice, in decreasing order.
        CellId maxCheck,
        CellId blockSize,
        size_t log2BucketCount);
#endif
    // Compute cell LSH signatures and store them.
    void computeLshSignatures(
        const string& geneSetName,      // The name of the gene set to be used.
        const string& cellSetName,      // The name of the cell set to be used.
        const string& lshName,          // The name of the Lsh object to be created.
        size_t lshCount,                // The number of LSH vectors to use.
        unsigned int seed               // The seed used to generate the LSH vectors.
        );


    // Analyze the quality of the LSH computation of cell similarity.
    void analyzeLsh(
        const string& geneSetName,      // The name of the gene set to be used.
        const string& cellSetName,      // The name of the cell set to be used.
        size_t lshCount,                // The number of LSH vectors to use.
        unsigned int seed,              // The seed used to generate the LSH vectors and to downsample.
        double csvDownsample            // The fraction of pairs that will be included in the output spreadsheet.
        );

    // Analyze LSH signatures.
    void analyzeLshSignatures(
        const string& geneSetName,      // The name of the gene set to be used.
        const string& cellSetName,      // The name of the cell set to be used.
        size_t lshCount,                // The number of LSH vectors to use.
        unsigned int seed              // The seed used to generate the LSH vectors and to downsample.
        );

    // Dump cell to csv file a set of similar cell pairs.
    void writeSimilarPairs(const string& name) const;

    // Analyze the quality of a set of similar pairs.
    void analyzeSimilarPairs(
        const string& name,
        double csvDownsample) const;

    // Compare two SimilarPairs objects computed using LSH,
    // assuming that the first one was computed using a complete
    // loop on all pairs (findSimilarPairs4).
    void compareSimilarPairs(
        const string& similarPairsName0,
        const string& similarPairsName1);

    // Remove a similar pairs object given its name.
    // This throws an exception if the requested SimilarPairs object does not exist.
    void removeSimilarPairs(const string& name);


    // Create a new cell graph.
    // Graphs are not persistent (they are stored in memory only).
    void createCellGraph(
        const string& graphName,            // The name of the graph to be created. This is used as a key in the graph map.
        const string& cellSetName,          // The cell set to be used.
        const string& similarPairsName,     // The name of the SimilarPairs object to be used to create the graph.
        double similarityThreshold,         // The minimum similarity to create an edge.
        size_t k,                           // The maximum number of neighbors (k of the k-NN graph).
        bool keepIsolatedVertices
     );

    // Unit test for class ExpressionMatrixSubset.
    void testExpressionMatrixSubset(CellId, CellId) const;

    // Find pairs of similar genes.
    void findSimilarGenePairs0(
        const string& geneSetName,
        const string& cellSetName,
        NormalizationMethod,
        const string& similarGenePairsName,
        size_t k,                   // The maximum number of similar genes pairs to be stored for each gene.
        double similarityThreshold
        );


    // Signature graphs.
    // All cells with the same signature are aggregated
    // into a single vertex of a signature graph.
    map<string, shared_ptr<SignatureGraph> > signatureGraphs;
    void createSignatureGraph(
        const string& signatureGraphName,
        const string& cellSetName,
        const string& lshName,
        size_t minCellCount);
    void removeSignatureGraph(const string& signatureGraphName);

    // Check that a signature graph does not exist,
    // and throw an exception if it does.
    void checkSignatureGraphDoesNotExist(const string& signatureGraphName) const;

    // Return a reference to a signature graph with a given name,
    // and throw and exception if not found.
    SignatureGraph& getSignatureGraph(const string& signatureGraphName);

private:
    // Functions to color a signature graph.
    // These set the color field of the vertices.
    void colorBlack(SignatureGraph&) const;
    void colorRandom(SignatureGraph&) const;
    void colorByMetaDataInterpretedAsCategory(SignatureGraph&, const string& metaDataName) const;
    void colorByMetaDataInterpretedAsColor(SignatureGraph&, const string& metaDataName) const;
    void colorByMetaDataInterpretedAsNumber(SignatureGraph&, const string& metaDataName) const;



    // The directory that contains the binary data for this Expression matrix.
    string directoryName;

    // A StringTable containing the gene names.
    // Given a GeneId (an integer), it can find the gene name.
    // Given the gene name, it can find the corresponding GeneId.
    MemoryMapped::StringTable<GeneId> geneNames;
public:

    // Return the name of the gene with the given id.
    string geneName(GeneId) const;
private:



    // The meta data for each gene.
    // For each gene we store pairs of string ids for each meta data (name, value) pair.
    // The corresponding strings are stored in geneMetaDataNames and geneMetaDataValues.
    // The first (name, value) pair for each gene contains name = "GeneName"
    // and value = the name of the gene.
    MemoryMapped::VectorOfLists< pair<StringId, StringId> > geneMetaData;
    MemoryMapped::StringTable<StringId> geneMetaDataNames;
    MemoryMapped::StringTable<StringId> geneMetaDataValues;

    // The number of genes that use each of the gene meta data names.
    // This is maintained to always have the same size as geneMetaDataNames,
    // and it is indexed by the StringId.
    MemoryMapped::Vector<GeneId> geneMetaDataNamesUsageCount;
    void incrementGeneMetaDataNameUsageCount(StringId);
    void decrementGeneMetaDataNameUsageCount(StringId);

    // Set a meta data (name, value) pair for a given gene.
    // If the name already exists for that gene, the value is replaced.
public:
    void setGeneMetaData(const string& geneName, const string& name, const string& value);
private:
    void setGeneMetaData(GeneId, const string& name, const string& value);
    void setGeneMetaData(GeneId, StringId nameId, const string& value);
    void setGeneMetaData(GeneId, StringId nameId, StringId valueId);


    // Vector containing fixed size information for each cell.
    // Variable size information (meta data and expression counts)
    // are stored separately - see below.
    MemoryMapped::Vector<Cell> cells;

    // A StringTable containing the cell names.
    // Given a CellId (an integer), it can find the cell name.
    // Given the cell name, it can find the corresponding CellId.
    // The name of reach cell is also stored as the first entry
    // in the meta data for the cell, called "cellName".
    MemoryMapped::StringTable<CellId> cellNames;

    // The meta data for each cell.
    // For each cell we store pairs of string ids for each meta data (name, value) pair.
    // The corresponding strings are stored in cellMetaDataNames and cellMetaDataValues.
    // The first (name, value) pair for each cell contains name = "CellName"
    // and value = the name of the cell.
    MemoryMapped::VectorOfLists< pair<StringId, StringId> > cellMetaData;
    MemoryMapped::StringTable<StringId> cellMetaDataNames;
    MemoryMapped::StringTable<StringId> cellMetaDataValues;

    // The number of cells that use each of the cell meta data names.
    // This is maintained to always have the same size as cellMetaDataNames,
    // and it is indexed by the StringId.
    MemoryMapped::Vector<CellId> cellMetaDataNamesUsageCount;
    void incrementCellMetaDataNameUsageCount(StringId);
    void decrementCellMetaDataNameUsageCount(StringId);

    // The expression counts for each cell. Stored in sparse format,
    // each with the GeneId it corresponds to.
    // For each cell, they are stored sorted by increasing GeneId.
    // This is indexed by the CellId.
    MemoryMapped::VectorOfVectors<pair<GeneId, float>, uint64_t> cellExpressionCounts;



public:
    // Accessors for expression counts, mostly used in Python.
    // The ones that specify a gene id or gene name can be slow,
    // as they require binary searches.

    // Get the expression count for a given cell and gene.
    // This can be zero.
    float getCellExpressionCount(CellId, GeneId) const;

    // Same as above, specifying a gene name instead of a GeneId.
    float getCellExpressionCount(CellId, const string& GeneName) const;

    // Get all the non-zero expression counts for a given cell.
    vector< pair<GeneId, float> > getCellExpressionCounts(CellId) const;

    // Get the expression count for a given gene, for a specified set of cells.
    // Each position in the returned vector has the count for
    // the cell at the same position in the input vector.
    // Some of the returned counts can be zero.
    vector<float> getCellsExpressionCount(const vector<CellId>&, GeneId) const;

    // Same as above, specifying a gene name instead of a GeneId.
    vector<float> getCellsExpressionCount(const vector<CellId>&, const string& geneName) const;

    // Get all the non-zero expression counts for a specified set of cells.
    // Each position in the returned vector has the counts for
    // the cell at the same position in the input vector.
    vector< vector< pair<GeneId, float> > > getCellsExpressionCounts(const vector<CellId>&) const;

    // Get all the non-zero expression counts for a specified set of cells,
    // but only including a specified set of genes.
    // Each position in the returned vector has the counts for
    // the cell at the same position in the input vector.
    // Note that in each returned pair<GeneId, float>, the geneId is
    // a global GeneId.
    vector< vector< pair<GeneId, float> > > getCellsExpressionCountsForGenes(
        const vector<CellId>&,
        const vector<GeneId>&) const;

    // Get a dense representation of a subset of the expression matrix
    // corresponding to a given gene set and cell set.
    // The returned matrix is a numpy array with row-major memory layout (C-style),
    // with a number of rows equal to the number of cells in the specified cell set,
    // and a number of columns equal to the number of genes in the specified gene set.
    // This means that, because of C-style layout, the returned matrix
    // is indexed in python as m[cellId][geneId], where cellId and geneId
    // are ids local to the cell set and gene set respectively
    // (that is, they only equal global cell ids and gene ids
    // if the function is called for the AllCells and AllGenes sets).
    pybind11::array getDenseExpressionMatrix(
        const string& geneSetName,
        const string& cellSetName,
        NormalizationMethod);

private:



    // Compute the expression vector for a cell and a given GeneSet,
    // normalizing it as requested.
    // The expression vector contains pairs(local gene id, count).
    // The local gene id is an index in the GeneSet.
    void computeExpressionVector(
        CellId,
        const GeneSet&,
        NormalizationMethod,
        vector< pair<GeneId, float> >& expressionVector // The computed expression vector.
        ) const;



    // Functions used to implement HttpServer functionality.
public:
    void explore(const ServerParameters& serverParameters);
    void explore(uint16_t port, const string& docDirectory);
private:
    ServerParameters serverParameters;
    void processRequest(const vector<string>& request, ostream& html);
    typedef void (ExpressionMatrix::*ServerFunction)(const vector<string>& request, ostream& html);
    map<string, ServerFunction> serverFunctionTable;
    set<string> nonHtmlKeywords;
    void fillServerFunctionTable();
    void writeNavigation(ostream& html);
    // void writeNavigation(ostream& html, const string& text, const string& url, const string& toolTip = "");
    void writeNavigation(ostream& html, const string& title, const vector<pair <string, string> >&);
    void exploreSummary(const vector<string>& request, ostream& html);
    void exploreHashTableSummary(const vector<string>& request, ostream&);
    void exploreGene(const vector<string>& request, ostream& html);
    void exploreGeneInformationContent(const vector<string>& request, ostream& html);
    void exploreGeneSets(const vector<string>& request, ostream& html);
    void exploreGeneSet(const vector<string>& request, ostream& html);
    void removeGeneSet(const vector<string>& request, ostream& html);
    void createGeneSetFromRegex(const vector<string>& request, ostream& html);
    void createGeneSetFromGeneNames(const vector<string>& request, ostream& html);
    void createGeneSetIntersectionOrUnion(const vector<string>& request, ostream& html);
    void createGeneSetDifference(const vector<string>& request, ostream& html);
    void createGeneSetUsingInformationContent(const vector<string>& request, ostream& html);
    void exploreCell(const vector<string>& request, ostream& html);
    ostream& writeCellLink(ostream&, CellId, bool writeId=false);
    ostream& writeCellLink(ostream&, const string& cellName, bool writeId=false);
    ostream& writeGeneLink(ostream&, GeneId, bool writeId=false);
    ostream& writeGeneLink(ostream&, const string& geneName, bool writeId=false);
    ostream& writeMetaDataSelection(ostream&, const string& selectName, bool multiple) const;
    ostream& writeMetaDataSelection(ostream&, const string& selectName, const set<string>& selected, bool multiple) const;
    ostream& writeMetaDataSelection(ostream&, const string& selectName, const vector<string>& selected, bool multiple) const;
    ostream& writeGeneMetaDataSelection(ostream&, const string& selectName, bool multiple) const;
    ostream& writeGeneMetaDataSelection(ostream&, const string& selectName, const set<string>& selected, bool multiple) const;
    ostream& writeGeneMetaDataSelection(ostream&, const string& selectName, const vector<string>& selected, bool multiple) const;
    void compareTwoCells(const vector<string>& request, ostream& html);
    void compareTwoGenes(const vector<string>& request, ostream& html);
    void exploreCellSets(const vector<string>& request, ostream& html);
    void exploreCellSet(const vector<string>& request, ostream& html);
    void createCellSetUsingMetaData(const vector<string>& request, ostream& html);
    void createCellSetUsingNumericMetaData(const vector<string>& request, ostream& html);
    void createCellSetIntersectionOrUnion(const vector<string>& request, ostream& html);
    void createCellSetDifference(const vector<string>& request, ostream& html);
    void downsampleCellSet(const vector<string>& request, ostream& html);
    ostream& writeCellSetSelection(ostream& html, const string& selectName, bool multiple) const;
    ostream& writeCellSetSelection(ostream& html, const string& selectName, const set<string>& selected, bool multiple) const;
    ostream& writeGeneSetSelection(ostream& html, const string& selectName, bool multiple) const;
    ostream& writeGeneSetSelection(ostream& html, const string& selectName, const set<string>& selected, bool multiple) const;
    ostream& writeCellGraphSelection(ostream& html, const string& selectName, bool multiple) const;
    ostream& writeNormalizationSelection(ostream& html, NormalizationMethod selectedNormalizationMethod) const;
    ostream& writeSimilarGenePairsSelection(ostream& html, const string& selectName) const;
    NormalizationMethod getNormalizationMethod(const vector<string>& request, NormalizationMethod defaultValue);
    void removeCellSet(const vector<string>& request, ostream& html);
    void similarPairs(const vector<string>& request, ostream& html);
    void createSimilarPairs(const vector<string>& request, ostream& html);
    void removeSimilarPairs(const vector<string>& request, ostream& html);
    void exploreCellGraphs(const vector<string>& request, ostream& html);
    void compareCellGraphs(const vector<string>& request, ostream& html);
    void exploreCellGraph(const vector<string>& request, ostream& html);
    // void clusterDialog(const vector<string>& request, ostream& html);
    // void cluster(const vector<string>& request, ostream& html);
    void createCellGraph(const vector<string>& request, ostream& html);
    void removeCellGraph(const vector<string>& request, ostream& html);
    void getAvailableSimilarPairs(vector<string>&) const;
    void getAvailableSimilarGenePairs(vector<string>&) const;
    vector<string> getAvailableLsh() const;
    ostream& writeLshSelection(ostream&, const string& selectName) const;
    void exploreMetaData(const vector<string>& request, ostream& html);
    void metaDataHistogram(const vector<string>& request, ostream& html);
    void metaDataContingencyTable(const vector<string>& request, ostream& html);
    void removeMetaData(const vector<string>& request, ostream& html);
    void exploreClusterGraphs(const vector<string>& request, ostream& html);
    void exploreClusterGraph(const vector<string>& request, ostream& html);
    void exploreClusterGraphSvgWithLabels(const vector<string>& request, ostream& html);
    void exploreClusterGraphPdfWithLabels(const vector<string>& request, ostream& html);
    void createClusterGraphDialog(const vector<string>& request, ostream& html);
    void createClusterGraph(const vector<string>& request, ostream& html);
    void removeClusterGraph(const vector<string>& request, ostream& html);
    void exploreCluster(const vector<string>& request, ostream& html);
    void exploreClusterCells(const vector<string>& request, ostream& html);
    void compareClustersDialog(const vector<string>& request, ostream& html);
    void compareClusters(const vector<string>& request, ostream& html);
    void createMetaDataFromClusterGraph(const vector<string>& request, ostream& html);
    void exploreSignatureGraphs(const vector<string>& request, ostream& html);
    void exploreSignatureGraph(const vector<string>& request, ostream& html);
    void createSignatureGraph(const vector<string>& request, ostream& html);
    void removeSignatureGraph(const vector<string>& request, ostream& html);
    void exploreGeneGraphs(const vector<string>& request, ostream& html);
    void exploreGeneGraph(const vector<string>& request, ostream& html);
    void createGeneGraph(const vector<string>& request, ostream& html);
    void removeGeneGraph(const vector<string>& request, ostream& html);


    // Class used by exploreGene.
    class ExploreGeneData {
    public:
        CellId cellId;
        float rawCount;
        float count1;   // L1 normalized.
        float count2;   // L2 normalized.
        bool operator<(const ExploreGeneData& that) const
        {
            return count2 > that.count2;    // Greater counts comes first
        }
    };

public:
    // Return a cell id given a string.
    // The string can be a cell name or a CellId (an integer).
    // Returns invalidCellId if the cell was not found.
    CellId cellIdFromString(const string&);

    // Return a gene id given a string.
    // The string can be a gene name or GeneId (a string).
    // Returns invalidGeneId if the gene was not found.
    GeneId geneIdFromString(const string&) const;

    // Return a gene id given its name.
    // Returns invalidGeneId if the gene was not found.
    GeneId geneIdFromName(const string&) const;

private:
    // Functionality to define and maintain cell sets.
    CellSets cellSets;

public:

    // Get the names of all currently defined cell sets.
    vector<string> getCellSetNames() const;

    // Get a copy of the cell set with a given name, for use in the Python API.
    // Returns an empty cell set if a cell set with the specified name does not exist.
    vector<CellId> getCellSet(const string& cellSetName) const;

    // Return a reference to the cell set with a given name,
    // throwing an exception if it does not exist.
    CellSet& cellSet(const string&);
    const CellSet& cellSet(const string&) const;

    // Check that a cell set with the given name does not exist,
    // and throw an exception if it does exist.
    void checkCellSetDoesNotExist(const string&) const;

    // Removes an existing cell set.
    void removeCellSet(const string&);



    // Gene sets, keyed by gene set name.
    // This always contains gene set AllGenes.
    map<string, GeneSet> geneSets;

    // Returns the names of the gene sets in the geneSets map that are identical
    // to the gene set of a SimilarPairs object with the given name.
    // Note that there could be zero, one, or multiple gene sets
    // that satisfy this condition.
    vector<string> geneSetNamesFromSimilarPairsName(const string& similarPairsName) const;

    // Removes an existing gene set.
    void removeGeneSet(const string&);


    // Compute gene information content in bits for a given gene set and cell set,
    // using the specified normalization method.
    void computeGeneInformationContent(
        const GeneSet&,
        const CellSet&,
        NormalizationMethod,
        vector<float>& geneInformationContent) const;

    // Same, for a single gene.
    float computeGeneInformationContent(GeneId, const CellSet&, NormalizationMethod) const;



    // Functions used for Locality Sensitive Hashing (LSH).
    // LSH is used for efficiently finding pairs of similar cells.
    // See Chapter 3 of Leskovec, Rajaraman, Ullman, Mining of Massive Datasets,
    // Cambridge University Press, 2014, also freely downloadable here:
    // http://www.mmds.org/#ver21
    // and in particular sections 3.4 through 3.7.



    // Create a new gene set consisting of genes whose name matches a given regular expression.
    bool createGeneSetFromRegex(const string& geneSetName, const string& regex);

    // Create a gene set consisting of the genes with names passed in a vector.
    // Names that don't correspond to valid gene names are ignored.
    // Returns true if successful, false if the specified gene set already exists.
    bool createGeneSetFromGeneNames(
        const string& geneSetName,
        const vector<string>& geneNames,
        int& ignoredCount,
        int& emptyCount);
    // Same as above, but throw an exception if any of the gene names are empty
    // or do not correspond to any gene.
    void createGeneSetFromGeneNames(
        const string& geneSetName,
        const vector<string>& geneNames);

    // Create a gene set consisting of the genes with ids passed in a vector.
    void createGeneSetFromGeneIds(
        const string& geneSetName,
        const vector<GeneId>&);

    // Create a new cell set that contains cells for which
    // the value of a specified meta data field is identical to a given string
    // or matches a given regular expression.
    // Return true if successful, false if a cell set with
    // the specified name already exists.
    void createCellSetUsingMetaData(
        const string& cellSetName,          // The name of the cell set to be created.
        const string& metaDataFieldName,    // The name of the meta data field to be used.
        const string&,                      // The string or regular expression that must be matched.
        bool useRegex                       // true=match as regular expression, false=match as string.
        );

    // Create a new cell set directly, using a vector CellIds.
    void createCellSet(
        const string& cellSetName,
        vector<CellId>& cellIds
        );


    // Create a new cell set consisting of cells for which a given meta data field
    // is numeric and is greater than, less than, or between specified values.
    void createCellSetUsingNumericMetaDataGreaterThan(
        const string& cellSetName,          // The name of the cell set to be created.
        const string& metaDataFieldName,
        double lowerBound);
    void createCellSetUsingNumericMetaDataLessThan(
        const string& cellSetName,          // The name of the cell set to be created.
        const string& metaDataFieldName,
        double upperBound);
    void createCellSetUsingNumericMetaDataBetween(
        const string& cellSetName,          // The name of the cell set to be created.
        const string& metaDataFieldName,
        double lowerBound,
        double upperBound);
    void createCellSetUsingNumericMetaData(
        const string& cellSetName,          // The name of the cell set to be created.
        const string& metaDataFieldName,
        bool useLowerBound, double lowerBound,
        bool useUpperBound, double upperBound);



    // Create a new cell set as the intersection or union of two or more existing cell sets.
    // The input cell sets are specified comma separated in the first argument.
    void createCellSetIntersection(const string& inputSets, const string& outputSet);
    void createCellSetUnion(const string& inputSets, const string& outputSet);
    void createCellSetIntersectionOrUnion(const string& inputSets, const string& outputSet, bool doUnion);

    // Create a new cell set as the difference between two existing cell sets.
    // Return true if successful, false if one of the input cell sets does not exist
    // or the output cell set already exists.
    void createCellSetDifference(const string& inputSet0, const string& inputSet1, const string& outputSet);

    // Create a new cell set by downsampling an existing cell set.
    void downsampleCellSet(
        const string& inputCellSetName,
        const string& newCellSetName,
        double probability,
        int seed);

    // The cell similarity graphs.
    // This is not persistent (lives in memory only).
    map<string, pair<CellGraphInformation, shared_ptr<CellGraph> > > cellGraphs;

    // Get the names of all currently defined cell similarity graphs.
    vector<string> getCellGraphNames() const;

    // Compute the layout (vertex positions) for the cell graph with a given name.
    void computeCellGraphLayout(const string& graphName);

    // Return vertex information for the cell graph with a given name.
    vector<CellGraphVertexInfo> getCellGraphVertices(const string& graphName) const;

    // Return the cell ids of the two vertices corresponding to
    // each of the edges of the cell graph with given name.
    vector< pair<CellId, CellId> > getCellGraphEdges(const string& graphName) const;

    // Store the cluster ids in a cell graph in a meta data field.
    void storeClusterId(const string& metaDataName, const CellGraph&);


    // The cluster graphs.
    // This is not persistent (lives in memory only).
    map<string, shared_ptr<ClusterGraph> > clusterGraphs;

    // Create a new named ClusterGraph by running clustering on an existing CellGraph.
    void createClusterGraph(
        ostream&,                               // For log output.
        const string& cellGraphName,            // The name of the cell graph to do clustering on.
        const ClusterGraphCreationParameters&,  // Parameters for the clustering algorithm.
        const string& clusterGraphName          // The name of the ClusterGraph to be created.
     );
    void createClusterGraph(
        const string& cellGraphName,            // The name of the cell graph to do clustering on.
        const ClusterGraphCreationParameters&,  // Parameters for the clustering algorithm.
        const string& clusterGraphName          // The name of the ClusterGraph to be created.
     );
    void createClusterGraph(
        const string& cellGraphName,            // The name of the cell graph to do clustering on.
        const string& clusterGraphName,         // The name of the ClusterGraph to be created.
        size_t stableIterationCount,            // Stop after this many iterations without changes.
        size_t maxIterationCount,               // Stop after this many iterations no matter what.
        size_t seed,                            // To initialize label propagation algorithm.
        size_t minClusterSize,                  // Minimum number of cells for a cluster to be retained.
        size_t maxConnectivity,
        double similarityThreshold,             // To remove edges of the cluster graph.
        double similarityThresholdForMerge      // To merge vertices of the cluster graph.
     );

    // Compute layouts for a named cluster graph.
    void computeClusterGraphLayout(const string& clusterGraphName, size_t timeoutSeconds, bool withLabels);

    // Get a vector of cluster ids for the vertices of a named cluster graph.
    vector<uint32_t> getClusterGraphVertices(const string& clusterGraphName) const;

    // Get the global GeneId's of the genes used by a ClusterGraph.
    vector<GeneId> getClusterGraphGenes(const string& clusterGraphName) const;

    // Get a vector of the cell ids in a given cluster.
    vector<CellId> getClusterCells(const string& clusterGraphName, uint32_t clusterId) const;

    // Get the average expression vector(L2-normalized) in a cluster.
    // The entries in the returned vector correspond on-on-one
    // to the gene ids returned by getClusterGraphGenes.
    vector<double> getClusterAverageExpression(const string& clusterGraphName, uint32_t clusterId) const;

    // Create meta data from the cluster ids stored in a ClusterGraph.
    void createMetaDataFromClusterGraph(
        const string& clusterGraphName,
        const string& metaDataName);



    // Gene graphs and related functionality.
    map<string, shared_ptr<GeneGraph> > geneGraphs;

    // Create or remove a gene graph.
    void createGeneGraph(
        ostream& out,
        const string& geneGraphName,
        const string& geneSetName,
        const string& similarGenePairsName,
        int k,
        double similarityThreshold);
    void removeGeneGraph(const string& geneGraphName);

    // Return a reference to the gene graph with a given name,
    // and throw and exception if not found.
    GeneGraph& getGeneGraph(const string& GeneGraphName);

    // Check that a gene graph does not exist,
    // and throw an exception if it does.
    void checkGeneGraphDoesNotExist(const string& geneGraphName) const;

    // Functions to color the gene graph.
    void colorGeneGraphBlack(GeneGraph&) const;
    void colorGeneGraphByMetaData(GeneGraph&, const string& metaDataName) const;

    // Return the value of a specified meta data field for a given gene.
    // Returns an empty string if the gene does not have the specified meta data field.
    string getGeneMetaData(GeneId, const string& name) const;
    string getGeneMetaData(GeneId, StringId) const;

};



#endif
