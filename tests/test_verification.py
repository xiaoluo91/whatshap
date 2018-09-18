from whatshap.core import PedigreeDPTable, Pedigree, NumericSampleIds, GenotypeLikelihoods, Genotype
from whatshap.testhelpers import string_to_readset, matrix_to_readset
from whatshap.verification import verify_mec_score_and_partitioning


def verify(rs, all_heterozygous=False):
	positions = rs.get_positions()
	recombcost = [1] * len(positions) # recombination costs 1, should not occur
	pedigree = Pedigree(NumericSampleIds(), 2)
	genotype_likelihoods = [None if all_heterozygous else GenotypeLikelihoods(2,2,[0,0,0])] * len(positions)
	pedigree.add_individual('individual0', [Genotype([0,1])] * len(positions), genotype_likelihoods) # all genotypes heterozygous
	dp_table = PedigreeDPTable(rs, recombcost, pedigree, 2, distrust_genotypes=not all_heterozygous)
	verify_mec_score_and_partitioning(dp_table, rs)


def test_string():
	reads = """
	  0             0
	  110111111111
	  00100
	       0001000000
	       000
	        10100
	              101
	"""
	rs = string_to_readset(reads)
	verify(rs, True)
	verify(rs, False)


def test_matrix():
	with open('tests/test.matrix') as f:
		rs = matrix_to_readset(f)
	verify(rs, True)
	verify(rs, False)
