import os

from microscopes.lda import model, runner
from microscopes.lda.definition import model_definition
from microscopes.common.rng import rng
from microscopes.lda import utils

# Based on test_lda_reuters.py in ariddell's lda
# https://github.com/ariddell/lda/blob/57f721b05ffbdec5cb11c2533f72aa1f9e6ed12d/lda/tests/test_lda_reuters.py


class TestLDANewsReuters():

    def _load_docs(self):
        test_dir = os.path.dirname(__file__)
        reuters_ldac_fn = os.path.join(test_dir, 'data', 'reuters.ldac')
        with open(reuters_ldac_fn, 'r') as f:
            self.docs = utils.docs_from_ldac(f)

        self.V = utils.num_terms(self.docs)
        self.N = len(self.docs)

    def setUp(self):
        self._load_docs()
        self.niters = 100

        self.defn = model_definition(self.N, self.V)
        self.prng = rng(seed=12345)
        self.latent = model.initialize(self.defn, self.docs, self.prng)
        self.r = runner.runner(self.defn, self.docs, self.latent)
        self.r.run(self.prng, self.niters)
        self.doc_topic = self.latent.document_distribution()

    def test_lda_news(self):
        assert len(self.doc_topic) == len(self.docs)

    def test_lda_monotone(self):
        # run additional iterations, verify improvement in log likelihood
        original_perplexity = self.latent.perplexity()
        self.r.run(self.prng, self.niters)
        assert self.latent.perplexity() < original_perplexity

    def test_lda_zero_iter(self):
        # compare to model with 0 iterations
        prng2 = rng(seed=54321)
        latent2 = model.initialize(self.defn, self.docs, prng2)
        assert latent2 is not None
        r2 = runner.runner(self.defn, self.docs, latent2)
        assert r2 is not None
        doc_topic2 = latent2.document_distribution()
        assert doc_topic2 is not None
        assert latent2.perplexity() > self.latent.perplexity()

    def test_lda_random_seed(self):
        # refit model with same random seed and verify results identical
        pass

    def test_lda_attributes(self):
        pass