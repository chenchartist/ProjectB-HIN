What is This Project?

This project finds groups of doctors who work together and treat many patients.

Example:

Doctor Smith treats patients A, B, C, D (treats 4 patients)
Doctor Johnson treats patients B, C, D, E (treats 4 patients)
They work together and form a community

The technical name is K-Core Community Search on Heterogeneous Information Networks (HINs).

The project analyzes hospital data including:

Patients (people being treated)
Doctors (healthcare providers)
Conditions (diseases/illnesses)
Medications (treatments)
Departments (where doctors work)
Setup Instructions

Step 1: Install Python

Go to: https://www.python.org/downloads/ Click the Download Python button Run the installer Check "Add Python to PATH" - this is important Click "Install Now" Restart your computer

Step 2: Download Project Files

Download these files from the project folder:

hin_kcore_search.py (main code)
hospital_data.csv (hospital data)
test_basic.py (simple example)
run_hospital.py (full hospital test)

Step 3: Verify Setup

Open PowerShell or Terminal and type:

python --version

You should see: Python 3.x.x

File Explanations
hin_kcore_search.py - MAIN CODE

This is the core algorithm that finds doctor communities. It takes patient-doctor relationships as input and finds groups of doctors who work together. You just use it, do not modify it. It has about 400 lines of code.

How to use it:

from hin_kcore_search import HINKCoreSearch

hin = HINKCoreSearch()

use it
hospital_data.csv - DATASET

This is the hospital patient and doctor data file.

Example of what is inside:

source,target,edge_type,source_type,target_type Patient_001,Doctor_Smith,treated_by,Patient,Doctor Patient_001,Condition_Diabetes,diagnosed_with,Patient,Condition Doctor_Smith,Department_Endocrinology,works_in,Doctor,Department

The columns are:

source: First person or thing
target: Second person or thing
edge_type: Type of relationship (treated_by, diagnosed_with, works_in)
source_type: What kind of thing source is (Patient, Doctor, Condition, etc.)
target_type: What kind of thing target is

Data included:

12 Patients
4 Doctors
6 Conditions
8 Medications
3 Departments
40 total relationships
test_basic.py - SIMPLE EXAMPLE

This is a learning example with small data (only 5 edges). Use this first to understand how the code works. It shows step by step how to use the project.

run_hospital.py - FULL TEST

This runs the algorithm on the hospital_data.csv file. It loads hospital data, shows network statistics, finds doctor communities, and prints results.
