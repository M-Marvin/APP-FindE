/*
* This is a simple tool for finding a matching E-series for provided resistor values (or other component values utilizing the E-series)
* Simply provide the required values as list to the executable when calling it in the terminal, plus -err followed by the required max. error (in percent)
* 
* Copyright 2024 M_Marvin (Discord, GitHub)
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/


#include <iostream>
#include <vector>
#include <map>
#include <math.h>
#include <strings.h>
#include <stdint.h>

// For setting the terminal mode
#include <io.h>
#include <fcntl.h>

using namespace std;

/* For historical reasons, these E-series do not match the actual equation, and need to be defined by fixed values */
static const double E3[] = {1.0, 2.2, 4.7};
static const double E6[] = {1.0, 1.5, 2.2, 3.3, 4.7, 6.8};
static const double E12[] = {1.0, 1.2, 1.5, 1.8, 2.2, 2.7, 3.3, 3.9, 4.7, 5.6, 6.8, 8.2};
static const double E24[] = {1.0, 1.1, 1.2, 1.3, 1.5, 1.6, 1.8, 2.0, 2.2, 2.4, 2.7, 3.0, 3.3, 3.6, 3.9, 4.3, 4.7, 5.1, 5.6, 6.2, 6.8, 7.5, 8.2, 9.1};

/*
* Transforms a value passed into a value in the range 0.0 - 10.0
* Example: 0.00456 -> 4.56	12300 -> 1.23
*/
double cutDown(double d) {
	if (d < 1.0)
		while (d < 1.0) d *= 10;
	else
		while (d > 10.0) d /= 10;
	return d;
}

/*
* Tries to find the first E-series, from which values the requested ratio can be made, while stayng below the requested maximal error.
* @param ratio The ratio of the two values
* @param maxError The maximum error that is acceptable
* @param error The actual error with the found values
* @param value1 The first value from the found series
* @param value2 The second value from the found series
* @returns The series from which the values where taken
*/
int findEseriesForRatio(double ratio, double maxError, double* error, double* value1, double* value2) {
	
	if (maxError <= 0.0) return 0;
	
	double r = cutDown(ratio);
	for (uint16_t n = 3; (uint32_t) n * 2 < 0xFFFF; n *= 2) {
		
		for (uint16_t e1 = 1; e1 <= n; e1++) {
			for (uint16_t e2 = 1; e2 <= e1; e2++) {
				
				const double* ser;
				switch (n) {
					case 3:
						ser = E3;
						goto fix_series;
					case 6:
						ser = E6;
						goto fix_series;
					case 12:
						ser = E12;
						goto fix_series;
					case 24:
						ser = E24;
						goto fix_series;

					fix_series:
						*value1 = ser[e1];
						*value2 = ser[e2];
						break;
						
					default:
						double r10 = pow(10, 1.0 / n);
						*value1 = round(pow(r10, e1) * 1000.0) / 1000.0;
						*value2 = round(pow(r10, e2) * 1000.0) / 1000.0;
						break;
				}
				
				double rat = *value1 / *value2;
				double err = abs(rat - r) / r;
				
				if (err <= maxError) {
					*error = err;
					while (*value1 / *value2 < ratio) *value1 *= 10;
					while (*value1 / *value2 > ratio) *value2 *= 10;
					return n;
				}
				
			}
		}
		
	}
	
	return 0;
	
}

/*
* Tries to find the first E-series, which's values are close to the provided values.
* @param values The values to find a close E-series for
* @param maxError The maximum error that is acceptable
* @param largestError Returns the largest error that occurs in the best E-series found
* @param seriesValues Returns a map that assigns each requested value (transformed to 0.0-10.0) the found E-series value
* @returns The best series found, or zero if none matched the maximum error
*/
int findEseries(vector<double>& values, double maxError, double* largestError, map<double, double>& seriesValues) {

	if (maxError <= 0.0) return 0;
	seriesValues.clear();

	double error = 0.0;
	uint16_t bestSeries = 0;
	for (uint16_t n = 3; (uint32_t) n * 2 < 0xFFFF; n *= 2) {

		*largestError = 0.0;
		const double* ser;

		switch (n) {
		case 3:
			ser = E3;
			goto fix_series;
		case 6:
			ser = E6;
			goto fix_series;
		case 12:
			ser = E12;
			goto fix_series;
		case 24:
			ser = E24;
			goto fix_series;

		fix_series:
			for (const auto value : values) {
				double v = cutDown(value);
				double smallestError = -1.0;
				for (uint16_t m = 0; m < n; m++) {
					double ve = ser[m];
					double err = abs(ve - v) / v;

					if (err < smallestError || smallestError < 0) {
						smallestError = err;
						seriesValues[v] = ve;
					}
				}
				if (smallestError > *largestError) {
					*largestError = smallestError;
				}
			}
			break;

		default:
			double r10 = pow(10, 1.0 / n);
			for (const auto value : values) {
				double v = cutDown(value);
				uint16_t m = round(log(v) / log(r10));
				double ve = round(pow(r10, m) * 1000.0) / 1000.0;
				double err = abs(ve - v) / v;

				seriesValues[v] = ve;

				if (err > *largestError) {
					*largestError = err;
				}
			}
			break;
		}

		if (*largestError < maxError) {
			bestSeries = n;
			error = *largestError;
			break;
		}
	
	}

	return bestSeries;

}

void findBestForRatio(double ratio, double maxError) {

	wprintf(L"╔═══════════════════════════════════════╗\n");
	wprintf(L"║                                       ║\n");
	wprintf(L"  \033[1Arequested max. error: \033[38;5;190m%.2lf %%\033[0m\n", maxError * 100.0);
	wprintf(L"╟───────────────────────────────────────╢\n");
	wprintf(L"║                                       ║\n");
	wprintf(L"  \033[1Atrying to find best E-series\n");
	wprintf(L"╚═══════════════════════════════════════╝\n");

	double error = 0.0;
	double value1 = 0.0;
	double value2 = 0.0;
	uint16_t series = findEseriesForRatio(ratio, maxError, &error, &value1, &value2);

	if (series == 0) {

		wprintf(L"╔═══════════════════════════════════════╗\n");
		wprintf(L"║                                       ║\n");
		wprintf(L"  \033[1A\033[38;5;196m[!] unabele to satisfy conditions\033[0m\n");
		wprintf(L"╚═══════════════════════════════════════╝\n");

		return;

	}

	wprintf(L"╔═══════════════════════════════════════╗\n");
	wprintf(L"║                                       ║\n");
	wprintf(L"  \033[1Abest series: \033[38;5;76mE%u\033[0m\n", series);
	wprintf(L"║                                       ║\n");
	wprintf(L"  \033[1Aerror: \033[38;5;190m%.2lf %%\033[0m\n", error * 100.0);
	wprintf(L"╟───────────────────────────────────────╢\n");
	wprintf(L"║ R_1        ┆ R_2        ┆ ratio       ║\n");

	wprintf(L"║            ┆            ┆             ║\n");
	wprintf(L"  \033[1A \033[38;5;76m%.3lf\033[0m\n", value1);
	wprintf(L"               \033[1A \033[38;5;76m%.3lf\033[0m\n", value2);
	wprintf(L"                            \033[1A \033[38;5;190m%.2lf\033[0m\n", value1 / value2);

	wprintf(L"╚═══════════════════════════════════════╝\n");

}

void findBestForValues(vector<double>& values, double maxError) {

	wprintf(L"╔═══════════════════════════════════════╗\n");
	wprintf(L"║                                       ║\n");
	wprintf(L"  \033[1Arequested max. error: \033[38;5;190m%.2lf %%\033[0m\n", maxError * 100.0);
	wprintf(L"╟───────────────────────────────────────╢\n");
	wprintf(L"║                                       ║\n");
	wprintf(L"  \033[1Atrying to find best E-series\n");
	wprintf(L"╚═══════════════════════════════════════╝\n");

	double largestError = 0.0;
	map<double, double> seriesValues = map<double, double>();
	uint16_t series = findEseries(values, maxError, &largestError, seriesValues);

	if (series == 0) {

		wprintf(L"╔═══════════════════════════════════════╗\n");
		wprintf(L"║                                       ║\n");
		wprintf(L"  \033[1A\033[38;5;196m[!] unabele to satisfy conditions\033[0m\n");
		wprintf(L"╚═══════════════════════════════════════╝\n");

		return;

	}

	wprintf(L"╔═══════════════════════════════════════╗\n");
	wprintf(L"║                                       ║\n");
	wprintf(L"  \033[1Abest series: \033[38;5;76mE%u\033[0m\n", series);
	wprintf(L"║                                       ║\n");
	wprintf(L"  \033[1Alargest error: \033[38;5;190m%.2lf %%\033[0m\n", largestError * 100.0);
	wprintf(L"╟───────────────────────────────────────╢\n");
	wprintf(L"║ R_orig     ┆ R_series   ┆ error       ║\n");

	for (map<double, double>::iterator i = seriesValues.begin(); i != seriesValues.end(); i++) {
	
		double err = abs(i->second - i->first) / i->first;

		wprintf(L"║            ┆            ┆             ║\n");
		wprintf(L"  \033[1A \033[38;5;76m%.3lf\033[0m\n", i->first);
		wprintf(L"               \033[1A \033[38;5;76m%.3lf\033[0m\n", i->second);
		wprintf(L"                            \033[1A \033[38;5;190m%.2lf %%\033[0m\n", err * 100.0);

	}

	wprintf(L"╚═══════════════════════════════════════╝\n");

}

int main(int argn, const char** argv) {

	// Setting the terminal mode
	_setmode(_fileno(stdout), _O_U16TEXT);

	wprintf(L"╔═══════════════════════════════════════╗\n");
	wprintf(L"║                                       ║\n");
	wprintf(L"  \033[1A\033[38;5;214mfind E tool by M_Marvin\033[0m\n");
	wprintf(L"╚═══════════════════════════════════════╝\n");
	
	// Read in max error and resistor values
	double maxError = 0.01;
	vector<double> values = vector<double>();
	bool ratioMode = false;
	bool parseValues = true;
	
	for (int i = 1; i < argn; i++) {
		string s = string(argv[i]);

		if (s == "-err") {
			if (argn <= i + 1) return -1;
			maxError = stod(string(argv[i + 1])) / 100.0;
			parseValues = false;
		} else if (s == "-ratio") {
			ratioMode = true;
			parseValues = false;
		} else if (parseValues) {
			values.push_back(stod(string(argv[i])));
		}
	}
	
	// Run actual algorithm to find best values
	if (ratioMode) {
		if (values.size() == 0) return -1;
		findBestForRatio(values[0], maxError);
	} else {
		findBestForValues(values, maxError);
	}
	
	return 0;
	
}
