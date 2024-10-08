#!/usr/bin/env python

__copyright__ = "Copyright 2018-2020, The PICRUSt Project"
__license__ = "GPL"
__version__ = "2.3.0-b"

from os import path
import shutil
import tempfile
import numpy as np
import pandas as pd
from math import ceil
from joblib import Parallel, delayed
from picrust2.util import system_call_check, TemporaryDirectory, call

def castor_hsp_workflow(tree_path,
                        trait_table_path,
                        hsp_method,
                        chunk_size=500,
                        calc_nsti=False,
                        calc_ci=False,
                        check_input=False,
                        num_proc=1,
                        ran_seed=None,
                        output=None,
                        verbose=False):
    '''Runs full HSP workflow. Main purpose is to read in trait table and run
    HSP on subsets of column at a time to be more memory efficient. Will return
    a single table of predictions and also a table of CIs (if specified).'''

    # Read in trait table as pandas dataframe.
    trait_tab = pd.read_csv(trait_table_path, sep="\t", dtype={'assembly': str})
    trait_tab.set_index('assembly', drop=True, inplace=True)

    # Calculate NSTI values if option set.
    if calc_nsti:
        nsti_values = castor_nsti(tree_path, trait_tab.index.values, verbose)

    # Create output directory for writing trait table subsets.
    with TemporaryDirectory() as temp_dir:

        num_chunks = int(trait_tab.shape[1]) / (chunk_size + 1)

        # Get all table subsets and write to file.
        # Also keep list of all temporary filenames.
        file_subsets = []

        for i in range(ceil(num_chunks)):

            subset_file = path.join(temp_dir, "subset_tab_" + str(i))

            subset_tab = trait_tab.iloc[:, i * chunk_size:(i + 1) * chunk_size]

            subset_tab.to_csv(path_or_buf=subset_file,
                              index_label="assembly",
                              sep="\t")

            file_subsets.append(subset_file)

        castor_out_raw = Parallel(n_jobs=num_proc, backend="threading")(delayed(
                                    castor_hsp_wrapper)(tree_path,
                                                        trait_in,
                                                        hsp_method,
                                                        calc_ci,
                                                        check_input,
                                                        ran_seed,
                                                        verbose)
                                    for trait_in in file_subsets)

    file_list = []
    file_obj = []
    for i in range(len(castor_out_raw)):
        file_list.append(path.join(castor_out_raw[i][0].name, "predicted_counts.txt"))
        file_obj.append(castor_out_raw[i][0])

    concat = path.join(path.dirname(path.abspath(__file__)),
                                  'concat')
    # Add NSTI as column as well if option specified.
    if calc_nsti:
        temp_obj = temporaryDirectory()
        temp_dir = temp_obj.name
        nsti_file = path.join(temp_dir, "nsti_values.txt")
        nsti_values.to_csv(path_or_buf=nsti_file, sep="\t", compression="infer")
        file_list.append(nsti_file)
        file_obj.append(temp_obj)
    concat_cmd = " ".join([concat,
                    ",".join(file_list),
                    output])

    # Run concat
    system_call_check(concat_cmd, print_command=verbose,
                        print_stdout=verbose, print_stderr=verbose)

    for i in file_obj:
        i.cleanup()

    ci_out_combined = None

    # if calc_ci:
    #     ci_out_combined = pd.concat(ci_out_chunks, axis=1, sort=True)

    return("", ci_out_combined)

class temporaryDirectory(object):
    def __init__(self, suffix=None, prefix=None, dir=None):
        self.name = tempfile.mkdtemp(suffix, prefix, dir)

    def __repr__(self):
        return "<{} {!r}>".format(self.__class__.__name__, self.name)

    def __enter__(self):
        return self.name

    def cleanup(self):
        # Line added by Gavin Douglas to change permissions to 777 before
        # deleting:
        call(["chmod", "-R", "777", self.name])

        shutil.rmtree(self.name)

def castor_hsp_wrapper(tree_path, trait_tab, hsp_method, calc_ci=False,
                       check_input=False, ran_seed=None, verbose=False):
    '''Wrapper for making system calls to castor_hsp.py Rscript.'''

    castor_hsp_script = path.join(path.dirname(path.abspath(__file__)),
                                  'Rscripts', 'castor_hsp.R')

    # Need to format boolean setting as string for R to read in as argument.
    if calc_ci:
        calc_ci_setting = "TRUE"
    else:
        calc_ci_setting = "FALSE"

    if check_input:
        check_input_setting = "TRUE"
    else:
        check_input_setting = "FALSE"

    # Create temporary directory for writing output files of castor_hsp.R
    temp_obj = temporaryDirectory()
    temp_dir = temp_obj.name

    output_count_path = path.join(temp_dir, "predicted_counts.txt")
    output_ci_path = path.join(temp_dir, "predicted_ci.txt")

    hsp_cmd = " ".join(["Rscript",
                        castor_hsp_script,
                        tree_path,
                        trait_tab,
                        hsp_method,
                        calc_ci_setting,
                        check_input_setting,
                        output_count_path,
                        output_ci_path,
                        str(ran_seed)])

    # Run castor_hsp.R
    system_call_check(hsp_cmd, print_command=verbose,
                        print_stdout=verbose, print_stderr=verbose)

    if calc_ci:
        asr_ci_table = pd.read_csv(filepath_or_buffer=output_ci_path,
                                    sep="\t", dtype={'sequence': str})
        asr_ci_table.set_index('sequence', drop=True, inplace=True)
    else:
        asr_ci_table = None

    # Return list with predicted counts and CIs.
    return [temp_obj, asr_ci_table]


def castor_nsti(tree_path,
                known_tips,
                verbose):
    '''Will calculate distance from each study sequence to the closest
    reference sequence. Takes in the path to treefile and the known tips
    (i.e. the rownames in the trait table - the reference genome ids).'''
    castor_nsti_script = path.join(path.dirname(path.abspath(__file__)),
                                   'Rscripts', 'castor_nsti.R')

    # Create temporary directory for working in.
    with TemporaryDirectory() as temp_dir:

        # Output known tip names to temp file
        # (note this object is a numpy.ndarray)
        known_tips_out = path.join(temp_dir, "known_tips.txt")
        known_tips.tofile(known_tips_out, sep="\n")

        nsti_tmp_out = path.join(temp_dir, "nsti_out.txt")

        # Run Rscript.
        system_call_check(" ".join(["Rscript",
                                    castor_nsti_script,
                                    tree_path,
                                    known_tips_out,
                                    nsti_tmp_out]),
                          print_command=verbose,
                          print_stdout=verbose,
                          print_stderr=verbose)


        # Read in calculated NSTI values.
        nsti_out = pd.read_csv(nsti_tmp_out, sep="\t", dtype={'sequence': str})
        nsti_out.set_index('sequence', drop=True, inplace=True)

    # Make sure that the table has the correct number of rows.
    if len(known_tips) != nsti_out.shape[0]:
        ValueError("Number of rows in returned NSTI table is incorrect.")

    return(nsti_out)
