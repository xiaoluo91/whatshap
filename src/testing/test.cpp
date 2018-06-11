#include "../readset.h"
#include "../read.h"
#include "../pedigree.h"
#include "../pedigreepartitions.h"
#include "../phredgenotypelikelihoods.h"
#include "../genotypedptable.h"
#include "../pedigree.h"
#include "../pedigreecolumncostcomputer.h"
#include "../genotypecolumncostcomputer.h"
#include "../columniterator.h"
#include "../entry.h"
#include "../transitionprobabilitycomputer.h"
#include "../vector2d.h"
#include "../graycodes.h"
#include "../columnindexingscheme.h"
#include "../columnindexingiterator.h"

#include <iostream>
#include <string>
#include <vector>
#include <list>
#include <sstream>

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

using namespace std;

size_t popcount(size_t x) {
    unsigned int count = 0;
    for (;x; x >>= 1) {
        count += x & 1;
    }
    return count;
}


ReadSet* string_to_readset(string s, string weights, bool use){
    ReadSet* read_set = new ReadSet;
    stringstream s1(s);
    stringstream s2(weights);
    string line;
    string line_weights;

    if(weights != ""){

    }
    unsigned int index = 0;
    while((std::getline(s1,line,'\n')) && (std::getline(s2,line_weights,'\n'))){
        if(line.length() == 0) continue;
        unsigned int counter = 0;
        Read* read = new Read("read"+std::to_string(index), 50, 0,0);
        for(unsigned int i = 0; i < line.length(); i++){
            counter += 1;
            if(line[i] == ' ') continue;
            unsigned int quality = int(line_weights[i] - '0');
            if(!use){
                read->addVariant((counter)*10,int(line[i] - '0'), quality);
            } else {
                read->addVariant((counter)*10,int(line[i] - '0'), 10);
            }

        }

        read_set->add(read);
        index += 1;
    }
    return read_set;
}

// extract the columns of the matrix as strings
vector<string> get_columns(string matrix, unsigned int col_count){
    stringstream ss(matrix);
    string line;
    vector<string> result(col_count,"");

    unsigned int index = 0;
    while(std::getline(ss,line,'\n')){
        for(unsigned int i = 0; i < col_count; i++){
            if(line[i] != ' ') result[i]+=line[i];
        }
    }

    return result;
}

long double naive_column_cost_computer(string current_column,unsigned int partitioning, unsigned int switch_cost, unsigned int alleles, unsigned int ploidy){
    long double result = 1.0L;
    vector<unsigned int> allele_to_partition;
    for(unsigned int j = 0; j < ploidy; j++){
        allele_to_partition.push_back((alleles >> j) & 1);
    }

    for(unsigned int j = 0; j < current_column.length(); j++){
        // check to which partition the current read belongs
        // check the entry in j-th bit
        unsigned int read_partition = partitioning % ploidy;
        if (int(current_column[j] - '0') != allele_to_partition[read_partition]) {
            result *= pow(10,-(long double)switch_cost/10.0L);
        } else {
            result *= 1-pow(10,-(long double)switch_cost/10.0L);
        }
        partitioning /= ploidy;
    }
    return result;
}

unsigned int naive_column_cost_computer_phred(string current_column,unsigned int p, unsigned int switch_cost, unsigned int ploidy){
    unsigned int result = numeric_limits < unsigned int >::max();
    unsigned int partitioning = p;
    for(unsigned int alleles = 0; alleles < pow(2,ploidy); ++alleles){
std::cout << "allele ass: " << alleles << std::endl;
        unsigned int assignment_cost = 0;
        vector<unsigned int> allele_to_partition;
        for(unsigned int j = 0; j < ploidy; j++){
            allele_to_partition.push_back((alleles >> j) & 1);
        }

        for(unsigned int j = 0; j < current_column.length(); j++){
            // check to which partition the current read belongs
            // check the entry in j-th bit
            unsigned int read_partition = partitioning % ploidy;
            if (int(current_column[j] - '0') != allele_to_partition[read_partition]) {
                assignment_cost += switch_cost;
            }
            partitioning /= ploidy;
        }
        if(assignment_cost < result) result = assignment_cost;
        partitioning = p;
    }
    return result;
}

// compare vector of entries to string
bool compare_entries(vector<const Entry*> c1, string c2){
    bool result = true;

    //for(unsigned int i = 0; i < c1.size(); i++)
    unsigned int i = 0;
    unsigned int j = 0;
    while((i<c1.size()) && (j<c2.length())){
        switch(c1[i]->get_allele_type()){
        case Entry::REF_ALLELE: if(c2[j] != '0'){result = false;} else {i+=1;j+=1;} break;
        case Entry::ALT_ALLELE: if(c2[j] != '1'){result = false;} else {i+=1;j+=1;} break;
        case Entry::BLANK: i += 1; break;
        default: break;
        }
    }
    return result;
}


TEST_CASE("test transition prob computer", "[test transition prob computer]"){
    vector<std::string> reads = {"11\n00", "10\n11", "00\n00", "10\n10", "01\n10"};
    std::string weights = "11\n11";

    SECTION("test trio uniform priors", "[test trio uniform]"){
        ReadSet* read_set = string_to_readset(reads[0],weights,false);
        std::vector<unsigned int>* positions = read_set->get_positions();
        std::vector<unsigned int> recombcost(positions->size(), 10);
        Pedigree* pedigree = new Pedigree();

        std::vector<PhredGenotypeLikelihoods*> gl_mother;
        std::vector<PhredGenotypeLikelihoods*> gl_father;
        std::vector<PhredGenotypeLikelihoods*> gl_child;

        for(unsigned int i = 0; i < positions->size(); i++){
            PhredGenotypeLikelihoods* n_m = new PhredGenotypeLikelihoods(1/3.0,1/3.0,1/3.0);
            PhredGenotypeLikelihoods* n_f = new PhredGenotypeLikelihoods(1/3.0,1/3.0,1/3.0);
            PhredGenotypeLikelihoods* n_c = new PhredGenotypeLikelihoods(1/3.0,1/3.0,1/3.0);
            gl_mother.push_back(n_m);
            gl_father.push_back(n_f);
            gl_child.push_back(n_c);
        }

        pedigree->addIndividual(0, std::vector<unsigned int >(positions->size(),1), gl_mother);
        pedigree->addIndividual(1, std::vector<unsigned int >(positions->size(),1), gl_father);
        pedigree->addIndividual(2, std::vector<unsigned int >(positions->size(),1), gl_child);
        pedigree->addRelationship(0,1,2);

        // create all pedigree partitions
        std::vector<PedigreePartitions*> pedigree_partitions;

        for(size_t i = 0; i < std::pow(4,pedigree->triple_count()); ++i)
        {
            pedigree_partitions.push_back(new PedigreePartitions(*pedigree,i,2));
        }

        for(unsigned int column_index = 0; column_index < positions->size(); ++column_index){
            TransitionProbabilityComputer trans(column_index,10,pedigree,pedigree_partitions);
            std::vector<long double> expected_cost = {0.9L*0.9L, 0.1L*0.9L, 0.1L*0.1L};
            long double nor = (0.9L*0.9L+2*0.1L*0.9L+0.1L*0.1L);

            for(unsigned int i = 0; i < 4; i++){
              long double sum = 0.0L;
              for(unsigned int j = 0; j < 4; j++){
                  unsigned int index = popcount(i ^ j);
                  REQUIRE((float)(expected_cost[index]/nor) == (float)trans.get_prob_transmission(i,j));
                  for(unsigned int a = 0; a < 16; a++){
                      if(j==0 || j==3){
                          if(a==6 || a==9){
                              REQUIRE(trans.get_prob_allele_assignment(j,a) == 1/30.0L);
                          } else {
                              REQUIRE(trans.get_prob_allele_assignment(j,a) == 1/15.0L);
                          }
                      } else {
                          if(a==5 || a==10){
                              REQUIRE(trans.get_prob_allele_assignment(j,a) == 1/30.0L);
                          } else {
                              REQUIRE(trans.get_prob_allele_assignment(j,a) == 1/15.0L);
                          }
                      }
                      sum += (float)trans.get_prob_transmission(i,j) * (float)trans.get_prob_allele_assignment(j,a);
                  }
                }
                REQUIRE(float(sum) == 1.0);
            }
        }

        delete read_set;
        delete positions;
        delete pedigree;
        for(unsigned int i=0; i < pedigree_partitions.size(); i++){
            delete pedigree_partitions[i];
        }

    }

    SECTION("test trio non-uniform priors", "[test trio non-uniform]"){
        ReadSet* read_set = string_to_readset(reads[0],weights,false);
        std::vector<unsigned int>* positions = read_set->get_positions();
        std::vector<unsigned int> recombcost(positions->size(), 10);
        Pedigree* pedigree = new Pedigree();

        std::vector<PhredGenotypeLikelihoods*> gl_mother;
        std::vector<PhredGenotypeLikelihoods*> gl_father;
        std::vector<PhredGenotypeLikelihoods*> gl_child;

        for(unsigned int i = 0; i < positions->size(); i++){
            PhredGenotypeLikelihoods* n_m = new PhredGenotypeLikelihoods(0,1,0);
            PhredGenotypeLikelihoods* n_f = new PhredGenotypeLikelihoods(0,1,0);
            PhredGenotypeLikelihoods* n_c = new PhredGenotypeLikelihoods(0.25,0.5,0.25);
            gl_mother.push_back(n_m);
            gl_father.push_back(n_f);
            gl_child.push_back(n_c);
        }

        pedigree->addIndividual(0, std::vector<unsigned int >(positions->size(),1), gl_mother);
        pedigree->addIndividual(1, std::vector<unsigned int >(positions->size(),1), gl_father);
        pedigree->addIndividual(2, std::vector<unsigned int >(positions->size(),1), gl_child);
        pedigree->addRelationship(0,1,2);

        // create all pedigree partitions
        std::vector<PedigreePartitions*> pedigree_partitions;

        for(size_t i = 0; i < std::pow(4,pedigree->triple_count()); ++i)
        {
            pedigree_partitions.push_back(new PedigreePartitions(*pedigree,i,2));
        }

        for(unsigned int column_index = 0; column_index < positions->size(); ++column_index){
            TransitionProbabilityComputer trans(column_index,10,pedigree,pedigree_partitions);
            std::vector<long double> expected_cost = {0.9L*0.9L, 0.1L*0.9L, 0.1L*0.1L};
            long double nor = (0.9L*0.9L+2*0.1L*0.9L+0.1L*0.1L);

            for(unsigned int i = 0; i < 4; i++){
              long double sum = 0.0L;
              for(unsigned int j = 0; j < 4; j++){
                  unsigned int index = popcount(i ^ j);
                  REQUIRE((float)(expected_cost[index]/nor) == (float)trans.get_prob_transmission(i,j));
                  for(unsigned int a = 0; a < 16; a++){
                      long double allele_prob = trans.get_prob_allele_assignment(j,a);
                      if(allele_prob != 0.0L){
                         REQUIRE(allele_prob == 1/4.0L);
                      }
                      sum += (float)trans.get_prob_transmission(i,j) * (float)trans.get_prob_allele_assignment(j,a);
                  }
                }
                REQUIRE(float(sum) == 1.0);
            }
        }

        delete read_set;
        delete positions;
        delete pedigree;
        for(unsigned int i=0; i < pedigree_partitions.size(); i++){
            delete pedigree_partitions[i];
        }

    }

   SECTION("test for single individual", "[test for single individual]"){
       ReadSet* read_set = string_to_readset(reads[0],weights,false);
       std::vector<unsigned int>* positions = read_set->get_positions();
       std::vector<unsigned int> recombcost(positions->size(), 10);
       Pedigree* pedigree = new Pedigree();

       std::vector<PhredGenotypeLikelihoods*> gl;
       for(unsigned int i = 0; i < positions->size(); i++){
           PhredGenotypeLikelihoods* n = new PhredGenotypeLikelihoods(1/3.0,1/3.0,1/3.0);
           gl.push_back(n);
       }

       pedigree->addIndividual(0, std::vector<unsigned int >(positions->size(),1), gl);
       std::vector<PedigreePartitions*> pedigree_partitions;


       for(size_t i = 0; i < std::pow(4,pedigree->triple_count()); ++i)
       {
           pedigree_partitions.push_back(new PedigreePartitions(*pedigree,i,2));
       }

       for(unsigned int column_index = 0; column_index < positions->size(); ++column_index){
           TransitionProbabilityComputer trans(column_index,10,pedigree,pedigree_partitions);
           for(unsigned int a = 0; a < 1<<pedigree_partitions[0]->count(); ++a){
               if((a>0) && (a<3)){
                   REQUIRE((trans.get_prob_transmission(0,0)*trans.get_prob_allele_assignment(0,a)) == 1.0L/6.0L);
               } else {
                   REQUIRE((trans.get_prob_transmission(0,0)*trans.get_prob_allele_assignment(0,a)) == 1.0L/3.0L);
               }
           }
       }

       delete read_set;
       delete positions;
       delete pedigree;
       for(unsigned int i=0; i < pedigree_partitions.size(); i++){
           delete pedigree_partitions[i];
       }
    }
}


TEST_CASE("test ColumnCostComputers","[test column_cost_computer]"){

    vector<std::string> reads = {"11\n00", "10\n11", "00\n00", "10\n10", "01\n10"};
    std::string weights = "11\n11";

    for(unsigned int r = 0; r < reads.size(); r++){
        ReadSet* read_set = string_to_readset(reads[r],weights,false);
	std::cout << read_set->toString() << std::endl;
        std::vector<unsigned int>* positions = read_set->get_positions();
        std::vector<PhredGenotypeLikelihoods*> genotype_likelihoods(positions->size(),nullptr);
        std::vector<unsigned int> recombcost(positions->size(), 1);
        Pedigree* pedigree = new Pedigree();
        pedigree->addIndividual(0, std::vector<unsigned int >(positions->size(),1), genotype_likelihoods);

        // create all pedigree partitions
        std::vector<PedigreePartitions*> pedigree_partitions;
        for(size_t i = 0; i < std::pow(4,pedigree->triple_count()); ++i)
        {
            pedigree_partitions.push_back(new PedigreePartitions(*pedigree,i,2));
        }

        // translate all individual ids to individual indices
         std::vector<unsigned int> read_sources;
        for(size_t i = 0; i<read_set->size(); ++i)
        {
            read_sources.push_back(pedigree->id_to_index(read_set->get(i)->getSampleID()));
        }
        vector<string> columns = get_columns(reads[r],2);

        ColumnIterator input_column_iterator(*read_set, positions);
        unsigned int col_ind = 0;

        while(input_column_iterator.has_next()){
            unique_ptr<vector<const Entry *> > current_input_column = input_column_iterator.get_next();

            // create column cost computer
            GenotypeColumnCostComputer cost_computer(*current_input_column, col_ind, read_sources, pedigree,*pedigree_partitions[0]);
            cost_computer.set_partitioning(0);

            unsigned int switch_cost = 1;

            // check if costs for initial partition (r1,r2/.) are computed correctly
            REQUIRE(cost_computer.get_cost(0) == naive_column_cost_computer(columns[col_ind],0,switch_cost,0,2));
            REQUIRE(cost_computer.get_cost(1) == naive_column_cost_computer(columns[col_ind],0,switch_cost,1,2));
            REQUIRE(cost_computer.get_cost(2) == naive_column_cost_computer(columns[col_ind],0,switch_cost,2,2));
            REQUIRE(cost_computer.get_cost(3) == naive_column_cost_computer(columns[col_ind],0,switch_cost,3,2));

            // switch first read (r2/r1)
            cost_computer.update_partitioning(0,1);
            REQUIRE(cost_computer.get_cost(0) == naive_column_cost_computer(columns[col_ind],1,switch_cost,0,2));
            REQUIRE(cost_computer.get_cost(1) == naive_column_cost_computer(columns[col_ind],1,switch_cost,1,2));
            REQUIRE(cost_computer.get_cost(2) == naive_column_cost_computer(columns[col_ind],1,switch_cost,2,2));
            REQUIRE(cost_computer.get_cost(3) == naive_column_cost_computer(columns[col_ind],1,switch_cost,3,2));

            // switch also second read (./r1,r2)
            cost_computer.update_partitioning(1,1);
            REQUIRE(cost_computer.get_cost(0) == naive_column_cost_computer(columns[col_ind],3,switch_cost,0,2));
            REQUIRE(cost_computer.get_cost(1) == naive_column_cost_computer(columns[col_ind],3,switch_cost,1,2));
            REQUIRE(cost_computer.get_cost(2) == naive_column_cost_computer(columns[col_ind],3,switch_cost,2,2));
            REQUIRE(cost_computer.get_cost(3) == naive_column_cost_computer(columns[col_ind],3,switch_cost,3,2));

            // test partition (r1/r2)
            cost_computer.set_partitioning(2);
            REQUIRE(cost_computer.get_cost(0) == naive_column_cost_computer(columns[col_ind],2,switch_cost,0,2));
            REQUIRE(cost_computer.get_cost(1) == naive_column_cost_computer(columns[col_ind],2,switch_cost,1,2));
            REQUIRE(cost_computer.get_cost(2) == naive_column_cost_computer(columns[col_ind],2,switch_cost,2,2));
            REQUIRE(cost_computer.get_cost(3) == naive_column_cost_computer(columns[col_ind],2,switch_cost,3,2));

            col_ind += 1;
        }

        delete read_set;
        delete positions;
        delete pedigree;
        for(unsigned int i=0; i < pedigree_partitions.size(); i++){
            delete pedigree_partitions[i];
        }
    }
}

TEST_CASE("test BackwardColumnIterator", "[test BackwardColumnIterator]") {

    SECTION("test small examples", "[test small examples]"){
        std::vector<std::string> matrices = {"10 \n010\n000", "01 \n000\n111", "0 1\n1 0\n 11"};
        std::vector<std::string> weights = {"11 \n111\n111", "11 \n111\n111", "1 1\n1 1\n 11"};

        for(unsigned int i = 0; i < matrices.size(); i++){
            ReadSet* read_set = string_to_readset(matrices[i], weights[i],false);
            vector<string> columns = get_columns(matrices[i],3);

            std::vector<unsigned int>* positions = read_set->get_positions();
            BackwardColumnIterator col_it(*read_set, positions);
            REQUIRE(col_it.has_next());

            // iterate backwards from end to start
            for(int j = 2; j >= 0; j--){
                auto col = col_it.get_next();
                REQUIRE(compare_entries(*col,columns[j]));
                if(j>0){
                    REQUIRE(col_it.has_next());
                } else {
                    REQUIRE(!col_it.has_next());
                }
            }

            // use jump column to iterate from end to start
            for(int j = 2; j >= 0; j--){
                col_it.jump_to_column(j);
                auto col = col_it.get_next();
                REQUIRE(compare_entries(*col,columns[j]));
                if(j>0){
                    REQUIRE(col_it.has_next());
                } else {
                    REQUIRE(!col_it.has_next());
                }
            }

            // use jump column to iterate from start to end
            for(int j = 0; j < 3; j++){
                col_it.jump_to_column(j);
                auto col = col_it.get_next();
                REQUIRE(compare_entries(*col,columns[j]));
            }

            delete read_set;
            delete positions;
        }
    }
}

TEST_CASE("test scaling of vector", "[test scaling of vector]"){
    Vector2D<long double> test(2,3,0.8L);
    test.divide_entries_by(0.8L);

    for(unsigned int i = 0; i < test.get_size0(); i++){
        for(unsigned int j = 0; j < test.get_size1(); j++) {
            REQUIRE(test.at(i,j) == 1L);
        }
    }
}

// helper function
void check_partitioning(std::vector<unsigned int>& readset1, std::vector<unsigned int>& readset2, std::vector<unsigned int>& readset3, std::vector<unsigned int>& forward_projections, std::vector<unsigned int>& backward_projections, unsigned int number_of_partitions){
    ColumnIndexingScheme scheme1(0, readset1, number_of_partitions);
    ColumnIndexingScheme scheme2(&scheme1, readset2, number_of_partitions);
    ColumnIndexingScheme scheme3(&scheme2, readset3, number_of_partitions);
    scheme1.set_next_column(&scheme2);
    scheme2.set_next_column(&scheme3);
    std::unique_ptr<ColumnIndexingIterator> iterator = scheme2.get_iterator();

    int pos_changed = 0;
    int partition_changed = 0;
    unsigned int i = 0;
    while(iterator->has_next()) {
        iterator->advance(&pos_changed, &partition_changed);
	unsigned int forward_index = forward_projections[i];
        unsigned int backward_index = backward_projections[i];
        REQUIRE(iterator->get_forward_projection() == forward_index);
        REQUIRE(iterator->index_forward_projection(iterator->get_index()) == forward_index);
        REQUIRE(iterator->get_backward_projection() == backward_index);
        REQUIRE(iterator->index_backward_projection(iterator->get_index()) == backward_index);
        i += 1;
    }
}


// checks ColumnIndexingIterator/ColumnIndexingScheme generalized to arbitrary partition numbers
TEST_CASE("test ColumnIndexingIterator", "[test ColumnIndexingIterator]"){

    SECTION("test bipartitions"){
        std::vector<std::vector<unsigned int>> expected_forward_projections = { {0,0,1,1,1,1,0,0,2,2,3,3,3,3,2,2}, {0,1,3,2,6,7,5,4,12,13,15,14,10,11,9,8}, {0,0,0,0,1,1,1,1} };
        std::vector<std::vector<unsigned int>> expected_backward_projections = { {0,1,3,2,2,3,1,0,0,1,3,2,2,3,1,0}, {0,1,3,2,6,7,5,4,12,13,15,14,10,11,9,8}, {0,1,1,0,0,1,1,0} };
        std::vector<std::vector<unsigned int>> read_ids1 = {{0,1},{0,1,2,3},{0}};
        std::vector<std::vector<unsigned int>> read_ids2 = {{0,1,2,3}, {0,1,2,3}, {0,1,2}};
        std::vector<std::vector<unsigned int>> read_ids3 = {{1,3}, {0,1,2,3}, {2}};

        for(unsigned int j = 0; j < read_ids1.size(); j++){
            check_partitioning(read_ids1[j], read_ids2[j], read_ids3[j], expected_forward_projections[j], expected_backward_projections[j], 2);
        }
    }

    SECTION("test tripartitions"){
        std::vector<std::vector<unsigned int>> expected_forward_projections = {{0,0,0,1,1,1,2,2,2},{0,1,2,5,4,3,6,7,8},{0,0,0,0,0,0,0,0,0}};
        std::vector<std::vector<unsigned int>> expected_backward_projections = {{0,1,2,2,1,0,0,1,2},{0,1,2,5,4,3,6,7,8},{0,0,0,0,0,0,0,0,0}};
        std::vector<std::vector<unsigned int>> read_ids1 = {{0},{0,1},{3}};
        std::vector<std::vector<unsigned int>> read_ids2 = {{0,1}, {0,1}, {0,1}};
        std::vector<std::vector<unsigned int>> read_ids3 = {{1}, {0,1}, {2}};

        for(unsigned int j = 0; j < read_ids1.size(); j++){
            check_partitioning(read_ids1[j], read_ids2[j], read_ids3[j], expected_forward_projections[j], expected_backward_projections[j], 3);
        }
    }

    SECTION("test 4-partitions"){
        std::vector<std::vector<unsigned int>> expected_forward_projections = {{0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3}};
        std::vector<std::vector<unsigned int>> expected_backward_projections = {{0,1,2,3,3,2,1,0,0,1,2,3,3,2,1,0}};
        std::vector<std::vector<unsigned int>> read_ids1 = {{0,1}};
        std::vector<std::vector<unsigned int>> read_ids2 = {{1,2}};
        std::vector<std::vector<unsigned int>> read_ids3 = {{2}};

        for(unsigned int j = 0; j < read_ids1.size(); j++){
            check_partitioning(read_ids1[j], read_ids2[j], read_ids3[j], expected_forward_projections[j], expected_backward_projections[j], 4);
        }
    }

}

TEST_CASE("test PedigreePartitions", "[test PedigreePartitions]"){

    SECTION("test diploid case"){
        Pedigree pedigree;
        pedigree.addIndividual(0, std::vector<unsigned int>(3,1), {0,0,0});
        pedigree.addIndividual(1, std::vector<unsigned int>(3,1), {0,0,0});
        pedigree.addIndividual(2, std::vector<unsigned int>(3,1), {0,0,0});
        pedigree.addRelationship(0,1,2);
        REQUIRE(pedigree.triple_count() == 1);

        PedigreePartitions pedigreepartitions(pedigree, 2, 2);
        REQUIRE(pedigreepartitions.count() == 4);
        REQUIRE(pedigreepartitions.haplotype_to_partition(0,0) == 0);
        REQUIRE(pedigreepartitions.haplotype_to_partition(0,1) == 1);
        REQUIRE(pedigreepartitions.haplotype_to_partition(1,0) == 2);
        REQUIRE(pedigreepartitions.haplotype_to_partition(1,1) == 3);
        REQUIRE(pedigreepartitions.haplotype_to_partition(2,0) == 1);
        REQUIRE(pedigreepartitions.haplotype_to_partition(2,1) == 2);
    }

    SECTION("test polyploid case"){
        Pedigree pedigree;
        pedigree.addIndividual(0, std::vector<unsigned int>(3,1), {0,0,0});
        pedigree.addIndividual(1, std::vector<unsigned int>(3,1), {0,0,0});
        pedigree.addIndividual(2, std::vector<unsigned int>(3,1), {0,0,0});
        pedigree.addRelationship(0,1,2);
        PedigreePartitions pedigreepartitions(pedigree,0,3);
       // due to higher ploidy, none of the partitions should have been merged
        REQUIRE(pedigreepartitions.count() == 9);
        REQUIRE(pedigreepartitions.haplotype_to_partition(0,0) == 0);
        REQUIRE(pedigreepartitions.haplotype_to_partition(0,1) == 1);
        REQUIRE(pedigreepartitions.haplotype_to_partition(0,2) == 2);
        REQUIRE(pedigreepartitions.haplotype_to_partition(1,0) == 3);
        REQUIRE(pedigreepartitions.haplotype_to_partition(1,1) == 4);
        REQUIRE(pedigreepartitions.haplotype_to_partition(1,2) == 5);
        REQUIRE(pedigreepartitions.haplotype_to_partition(2,0) == 6);
        REQUIRE(pedigreepartitions.haplotype_to_partition(2,1) == 7);
        REQUIRE(pedigreepartitions.haplotype_to_partition(2,2) == 8);
    }
}

TEST_CASE("test polyploid_column_costs","[test column_cost_computer]"){

    vector<std::string> reads = {"11\n00", "10\n11", "00\n00", "10\n10", "01\n10"};
    std::string weights = "11\n11";

    for(unsigned int r = 0; r < reads.size(); r++){
        ReadSet* read_set = string_to_readset(reads[r],weights,false);
        std::cout << read_set->toString() << std::endl;
        std::vector<unsigned int>* positions = read_set->get_positions();
        std::vector<PhredGenotypeLikelihoods*> genotype_likelihoods(positions->size(), new PhredGenotypeLikelihoods());
        std::vector<unsigned int> recombcost(positions->size(), 1);
        Pedigree* pedigree = new Pedigree();
        pedigree->addIndividual(0, std::vector<unsigned int >(positions->size(),1), genotype_likelihoods);

        // create all pedigree partitions
        std::vector<PedigreePartitions*> pedigree_partitions;
        for(size_t i = 0; i < std::pow(4,pedigree->triple_count()); ++i)
        {
            pedigree_partitions.push_back(new PedigreePartitions(*pedigree,i,3));
        }

        // translate all individual ids to individual indices
         std::vector<unsigned int> read_sources;
        for(size_t i = 0; i<read_set->size(); ++i)
        {
            read_sources.push_back(pedigree->id_to_index(read_set->get(i)->getSampleID()));
        }
        vector<string> columns = get_columns(reads[r],2);

        ColumnIterator input_column_iterator(*read_set, positions);
        unsigned int col_ind = 0;
        while(input_column_iterator.has_next()){
            unique_ptr<vector<const Entry *> > current_input_column = input_column_iterator.get_next();

            // create column cost computers (Genotype- + PedigreeColumnCostComputers)
            GenotypeColumnCostComputer cost_computer(*current_input_column, col_ind, read_sources, pedigree,*pedigree_partitions[0]);
            PedigreeColumnCostComputer phred_cost_computer(*current_input_column, col_ind, read_sources, pedigree, *pedigree_partitions[0], true);
            unsigned int switch_cost = 1;

            // check if costs are computed correctly for all allele assignments
            for(unsigned int partitioning = 0; partitioning < pow(3,2); ++partitioning){
                cost_computer.set_partitioning(partitioning);
                phred_cost_computer.set_partitioning(partitioning);
                std::cout << col_ind << " " << partitioning <<  " " << read_set->toString() << naive_column_cost_computer_phred(columns[col_ind], partitioning, switch_cost, 3) << " " << phred_cost_computer.get_cost() << std::endl;
                REQUIRE(phred_cost_computer.get_cost() == naive_column_cost_computer_phred(columns[col_ind], partitioning, switch_cost, 3));
                for(unsigned int allele_assignment = 0; allele_assignment < 8; ++allele_assignment){
                    REQUIRE(cost_computer.get_cost(allele_assignment) == naive_column_cost_computer(columns[col_ind], partitioning, switch_cost, allele_assignment, 3));
                }
            }
            col_ind += 1;
        }

        delete read_set;
        delete positions;
        delete pedigree;
        for(unsigned int i=0; i < pedigree_partitions.size(); i++){
            delete pedigree_partitions[i];
        }
        for(unsigned int i=0; i < genotype_likelihoods.size(); i++){
            delete genotype_likelihoods[i];
        }
    }
}

