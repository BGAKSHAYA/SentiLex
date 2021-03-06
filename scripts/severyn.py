#!/usr/bin/env python2.7
# -*- mode: python; coding: utf-8; -*-

"""Module for generating lexicon using Severyn and Moschitti's method (2014).

"""

##################################################################
# Imports
from __future__ import unicode_literals, print_function

from common import ENCODING, ESC_CHAR, FMAX, FMIN, \
    INFORMATIVE_TAGS, NEGATIVE, POSITIVE, NEUTRAL, SENT_END_RE, \
    TAB_RE, NONMATCH_RE, MIN_TOK_CNT, check_word, normalize

from collections import Counter
from sklearn.feature_extraction import DictVectorizer
from sklearn.pipeline import Pipeline
from sklearn.svm import LinearSVC
import codecs
import sys


##################################################################
# Constants
FASTMODE = False
POS_IDX = 0
NEG_IDX = 1
SUBJECTIVE = "subjective"


##################################################################
# Methods
def _toks2feats(a_tweet_toks):
    """Convert set of tweet's tokens to a dictionary of features.

    @param a_tweet_toks - set of tweet's uni- and bigrams

    @return feature dictionary with tweet's tokens as keys and values 1

    """
    return {w: 1. for w in a_tweet_toks}


def _update_ts(a_ts_x, a_ts_y, a_tweet_toks,
               a_pos, a_neg, a_neut,
               a_pos_re=NONMATCH_RE, a_neg_re=NONMATCH_RE):
    """Update training set of features and classes.

    @param a_ts_x - training set features
    @param a_ts_y - training set gold classes
    @param a_tweet_toks - set of tweet's uni- and bigrams
    @param a_pos - initial set of positive terms to be expanded
    @param a_neg - initial set of negative terms to be expanded
    @param a_neut - initial set of neutral terms to be expanded
    @param a_pos_re - regular expression for matching positive terms
    @param a_neg_re - regular expression for matching negative terms

    @return \c void

    @note modifies `a_ts' and `a_tweet_feats' in place

    """
    if not a_tweet_toks:
        return
    tweet = ' '.join([w
                      if isinstance(w, basestring)
                      else ' '.join(w)
                      for w in sorted(a_tweet_toks)])
    added = False
    if a_tweet_toks & a_pos or a_pos_re.search(tweet):
        a_ts_y.append(POSITIVE)
        added = True
    elif a_tweet_toks & a_neg or a_neg_re.search(tweet):
        a_ts_y.append(NEGATIVE)
        added = True
    elif a_tweet_toks & a_neut:
        a_ts_y.append(NEUTRAL)
        added = True
    if added:
        a_ts_x.append(_toks2feats(a_tweet_toks))
    a_tweet_toks.clear()


def _prune_ts(a_ts_x, a_ts_y):
    """Remove fetures with too low frequency.

    @param a_ts_x - list of extracted features
    @param a_ts_y - list of gold instance classes

    @return 2-tuple - pruned copies of `a_ts_x' and `a_ts_y'

    """
    tokstat = Counter(x_i
                      for x in a_ts_x
                      for x_i in x.iterkeys()
                      )
    x_ = None
    ts_x = []
    ts_y = []
    for x, y in zip(a_ts_x, a_ts_y):
        x_ = {k: v for k, v in x.iteritems()
              if tokstat[k] >= MIN_TOK_CNT
              }
        if x_:
            ts_x.append(x_)
            ts_y.append(y)
    return (ts_x, ts_y)


def _read_files(a_crp_files, a_pos, a_neg, a_neut,
                a_pos_re=NONMATCH_RE, a_neg_re=NONMATCH_RE):
    """Read corpus files and populate one-directional co-occurrences.

    @param a_crp_files - files of the original corpus
    @param a_pos - initial set of positive terms to be expanded
    @param a_neg - initial set of negative terms to be expanded
    @param a_neut - initial set of neutral terms to be expanded
    @param a_pos_re - regular expression for matching positive terms
    @param a_neg_re - regular expression for matching negative terms

    @return 2-tuple - training sets of features and their gold classes

    """
    print("Reading corpus...", end="", file=sys.stderr)
    i = 0
    ts_x = []
    ts_y = []
    tweet_toks = set()
    iform = itag = ilemma = prev_lemma = ""
    for ifname in a_crp_files:
        with codecs.open(ifname, 'r', ENCODING) as ifile:
            prev_lemma = ""
            for iline in ifile:
                iline = iline.strip().lower()
                if iline and iline[0] == ESC_CHAR:
                    if FASTMODE:
                        i += 1
                        if i > 300:
                            break
                    _update_ts(ts_x, ts_y, tweet_toks,
                               a_pos, a_neg, a_neut, a_pos_re, a_neg_re)
                    prev_lemma = ""
                    continue
                elif not iline or SENT_END_RE.match(iline):
                    prev_lemma = ""
                    continue
                try:
                    iform, itag, ilemma = TAB_RE.split(iline)
                except:
                    print("Invalid line format at line: {:s}".format(
                        repr(iline)), file=sys.stderr
                    )
                    continue
                ilemma = normalize(ilemma)
                if a_pos_re.search(iform) or a_neg_re.search(iform):
                    tweet_toks.add(iform)
                elif a_pos_re.search(ilemma) or a_neg_re.search(ilemma):
                    tweet_toks.add(ilemma)
                elif itag[:2] not in INFORMATIVE_TAGS \
                        or not check_word(ilemma):
                    continue
                else:
                    tweet_toks.add(ilemma)
                if prev_lemma:
                    tweet_toks.add((prev_lemma, ilemma))
                prev_lemma = ilemma
            _update_ts(ts_x, ts_y, tweet_toks,
                       a_pos, a_neg, a_neut, a_pos_re, a_neg_re)
    print(" done", file=sys.stderr)
    return _prune_ts(ts_x, ts_y)


def severyn(a_N, a_crp_files, a_pos, a_neg, a_neut,
            a_pos_re=NONMATCH_RE, a_neg_re=NONMATCH_RE):
    """Method for generating sentiment lexicons using Severyn's approach.

    @param a_N - number of terms to extract
    @param a_crp_files - files of the original corpus
    @param a_pos - initial set of positive terms to be expanded
    @param a_neg - initial set of negative terms to be expanded
    @param a_neut - initial set of neutral terms to be expanded
    @param a_pos_re - regular expression for matching positive terms
    @param a_neg_re - regular expression for matching negative terms

    @return list of terms sorted according to their polarity scores

    """
    a_pos = set(normalize(w) for w in a_pos)
    a_neg = set(normalize(w) for w in a_neg)

    vectorizer = DictVectorizer()
    # model for distinguishing between the subjective and objective classes
    so_clf = LinearSVC(C=0.3)
    so_model = Pipeline([("vectorizer", vectorizer),
                         ("LinearSVC", so_clf)])
    # model for distinguishing between the positive and negative classes
    pn_clf = LinearSVC(C=0.3)
    pn_model = Pipeline([("vectorizer", vectorizer),
                         ("LinearSVC", pn_clf)])
    # generate general training sets
    x_so = []
    y_so = []
    x_pn = []
    y_pn = []
    for x, y in zip(*_read_files(a_crp_files, a_pos, a_neg, a_neut,
                                 a_pos_re, a_neg_re)):
        if y != NEUTRAL:
            y_so.append(SUBJECTIVE)
            x_pn.append(x)
            y_pn.append(y)
        else:
            y_so.append(y)
        x_so.append(x)
    # generate lists of gold labels spefici for each task
    so_model.fit(x_so, y_so)
    # check whether the sign of core features is being determined correctly
    so_feat2coef = {}
    coefs = so_clf.coef_[0]
    for f_name, f_score in zip(vectorizer.get_feature_names(),
                               coefs):
        assert f_name not in a_neut or f_score < 0, \
            "Invalid coefficient sign expected for objective features."
        so_feat2coef[f_name] = f_score
    # train positive/negative classifier
    pn_model.fit(x_pn, y_pn)

    # generate actual output
    ret = [(w, POSITIVE, FMAX) for w in a_pos] \
        + [(w, NEGATIVE, FMIN) for w in a_neg]

    coefs = pn_clf.coef_[0]
    for f_name, f_score in zip(vectorizer.get_feature_names(),
                               coefs):
        assert f_name not in a_neg or f_score < 0
        # skip seed terms and terms deemed as objective
        if f_name in a_pos or f_name in a_neg \
           or so_feat2coef.get(f_name, 1) < 0:
            continue
        ret.append((f_name,
                    POSITIVE if f_score > 0. else NEGATIVE,
                    f_score * so_feat2coef.get(f_name, 1.)))
    ret.sort(key=lambda el: abs(el[-1]), reverse=True)
    if a_N >= 0:
        del ret[a_N:]
    return ret
