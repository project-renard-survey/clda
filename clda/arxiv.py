# -*- coding: utf-8 -*-

from __future__ import division, print_function

__all__ = ["ArxivReader"]

import string
import sqlite3
import operator
import numpy as np
from collections import defaultdict


class ArxivReader(object):

    def __init__(self, dbpath):
        self.dbpath = dbpath
        with sqlite3.connect(dbpath) as connection:
            c = connection.cursor()
            c.execute("SELECT count(*) FROM articles")
            self.nfiles = int(c.fetchone()[0])
        self.validation_set = []

    def iter_docs(self):
        while True:
            with sqlite3.connect(self.dbpath) as connection:
                c = connection.cursor()
                c.execute("SELECT * FROM articles WHERE rowid=?",
                          (np.random.randint(self.nfiles), ))
                doc = c.fetchone()
            if doc is None:
                continue
            # if "astro-ph" in doc[2]:
            yield doc

    def validation(self, n):
        docs = []
        for doc in self.iter_docs():
            if doc[0] in self.validation_set:
                continue
            words = self.parse_document(doc)
            if not len(words):
                continue
            self.validation_set.append(doc[0])
            docs.append(words)
            if len(docs) >= n:
                break
        return docs

    def parse_document(self, doc):
        return [self.vocab[w.lower()]
                for w in doc[3].split()+doc[4].split()
                if w.lower() in self.vocab]

    def generate_vocab(self):
        vocab = defaultdict(int)
        with sqlite3.connect(self.dbpath) as connection:
            c = connection.cursor()
            for doc in c.execute("SELECT * FROM articles"):
                for w in doc[2].split()+doc[3].split():
                    vocab[w.lower()] += 1
        return sorted(vocab.iteritems(), key=operator.itemgetter(1),
                      reverse=True)

    def load_vocab(self, fn, skip=0, nvocab=None, stopwords=None,
                   strip_punc=True, strip_numbers=True, strip_tex=False,
                   min_length=3):
        self.vocab = {}
        self.vocab_list = []
        with open(fn, "r") as f:
            for i, line in enumerate(f):
                cols = line.split()
                w = cols[0]
                if (i < skip or len(w) < min_length
                        or (stopwords is not None and w in stopwords)
                        or (strip_numbers and
                            not len(w.translate(None, str("0123456789.,~"))))
                        or (strip_punc and
                            not len(w.translate(None, string.punctuation)))
                        or (strip_tex and w.startswith("\\"))):
                    continue
                self.vocab_list.append(w.strip())
                self.vocab[w.strip()] = len(self.vocab_list) - 1
                if nvocab is not None and len(self.vocab_list) >= nvocab:
                    break

    def __iter__(self):
        for doc in self.iter_docs():
            if doc[0] in self.validation_set:
                continue
            words = self.parse_document(doc)
            if len(words):
                yield words

    def __getitem__(self, arxiv_id):
        with sqlite3.connect(self.dbpath) as connection:
            c = connection.cursor()
            c.execute("SELECT * FROM articles WHERE arxiv_id=?",
                      (arxiv_id, ))
            doc = c.fetchone()
        if doc is None:
            raise ValueError("No abstract with id='{0}'".format(arxiv_id))
        return doc


if __name__ == "__main__":
    reader = ArxivReader("data/abstracts.db")
    open("data/vocab.txt", "w").write("\n".join(map("{0[0]} {0[1]}".format,
                                                    reader.generate_vocab())))
