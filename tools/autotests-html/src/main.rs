use std::fs;
use std::path::Path;
use std::str::FromStr;
use std::fmt::Debug;
use build_html::{Html, HtmlContainer, TableCell, TableCellType, TableRow};
use serde::{Serialize, Deserialize, Deserializer};
use strum::{EnumString};

#[derive(Default, Debug, Copy, Clone)]
#[derive(Serialize, Deserialize)]
#[derive(EnumString)]
enum OperatorStatus {
       Up, // Test results have improved
     Down, // Test results are getting worse
     Pass, // All tests now pass simulation
    Break, // Break from previous pass
   Stable, // Test results are the same as before
   #[default] Undefined, // Could not resolve operator's status
}

/// This would be the HTML representation of the Operator Status:
impl ToString for OperatorStatus {
    fn to_string(&self) -> String {
        match self {
               Self::Up => String::from("&#8593"),
             Self::Down => String::from("&#8595"),
             Self::Pass => String::from("PASS!"),
            Self::Break => String::from("BREAK!"),
           Self::Stable => String::from("="),
        Self::Undefined => String::from("n/a")
        }
    }
}

impl OperatorStatus {
    fn color(&self) -> String {
        match self {
               Self::Up => String::from("green"),
             Self::Down => String::from("red"),
             Self::Pass => String::from("green"),
            Self::Break => String::from("red"),
           Self::Stable => String::from("blue"),
        Self::Undefined => String::from("black")
        }
    }
}

#[derive(Default, Debug)]
#[derive(Serialize, Deserialize)]
struct OperatorResults {
    // ---------------------------------------------------
    #[serde(rename = "Operator")]
    id: String,
    // ---------------------------------------------------
    // Note: the generated .csv file has ', ' as delimiter
    // which is impossible to set in the reader's options,
    // so it doesn't take the whitespace into account:
    #[serde(rename = " Tests")]
    #[serde(deserialize_with = "deserialize_trim")]
    ntests: usize,
    // ---------------------------------------------------
    #[serde(rename = " Generation")]
    #[serde(deserialize_with = "deserialize_trim")]
    generation: usize,
    // ---------------------------------------------------
    #[serde(rename = " Simulation")]
    #[serde(deserialize_with = "deserialize_trim")]
    simulation: usize,
    // ---------------------------------------------------
    #[serde(rename = " %")]
    #[serde(deserialize_with = "deserialize_trim")]
    percentage: f32,
    //----------------------------------------------------
    #[serde(rename = " Status")]
    #[serde(deserialize_with = "deserialize_trim")]
    status: OperatorStatus,
    // ---------------------------------------------------
    #[serde(rename = " Passed Last")]
    #[serde(deserialize_with = "deserialize_trim")]
    passed_last: String, // commit hash
}

use std::process::Command;

impl OperatorResults {
    /// Update the Operator's status, in comparison
    /// with `cmp`: a previous internal representation
    /// of the autotest results for the same operator.
    fn update(&mut self, cmp: &OperatorResults) {
        if cmp.percentage == 100f32
        && self.percentage < 100f32 {
        // If Operator was previously at 100%
        // but not anymore: break!
            self.status = OperatorStatus::Break;
        } else if cmp.percentage < 100f32
            // If operator now reaches 100%: pass!
             && self.percentage == 100f32 {
            self.status = OperatorStatus::Pass;
        } else if self.simulation > cmp.simulation {
            self.status = OperatorStatus::Up;
        } else if self.simulation < cmp.simulation {
            self.status = OperatorStatus::Down;
        } else {
            self.status = OperatorStatus::Stable;
        }
        println!("Updating Operator {}, status: {:?}",
            self.id, self.status
        );
        // If pass: save the commit hash
        if self.percentage == 100f32 {
            let out = Command::new("git")
                .arg("rev-parse").arg("HEAD")
                .output()
                .expect("Failed to get current git commit")
                .stdout;
            self.passed_last = String::from_utf8(out)
                .expect("Could not parse git output");
            self.passed_last.pop();
        }
    }
}

impl std::ops::AddAssign<&OperatorResults> for OperatorResults {
    fn add_assign(&mut self, other: &OperatorResults) {
        self.ntests += other.ntests;
        self.generation += other.generation;
        self.simulation += other.simulation;
    }
}

fn deserialize_trim<'de, D, T>(deserializer: D) -> Result<T, D::Error>
    where D: Deserializer<'de>,
          T: FromStr, <T as FromStr>::Err: Debug
{
    Ok(String::deserialize(deserializer)
        .unwrap()
        .trim()
        .parse::<T>()
        .unwrap()
    )
}

macro_rules! cell {
    ($contents:expr) => {
        TableCell::new(TableCellType::Data)
            .with_paragraph($contents)
    };
}

fn add_color(cell: &mut TableCell, txt: impl ToString, color: &str) {
    cell.add_paragraph_attr(
        txt.to_string().trim(), [(
            "style", format!("color:{color}").as_str()
        )]
    );
}

const gitlab: &str = "https://gitlab.com/flopoco/flopoco";

fn get_commit_date(commit: &String) -> String {
    let out = Command::new("git")
        .arg("show")
        .arg("-s")
        .arg("--format=%ci")
        .arg(&commit)
        .output()
        .expect("Could not get commit date")
        .stdout;
    return String::from_utf8(out).expect("Could not parse commit date");
}

fn add_html_row(op: &OperatorResults, table: &mut build_html::Table) {
    // For each row:
    // Note: TableCell doesn't implement copy/clone
    // so we have to instantiate each cell individually...
    let mut c0 = TableCell::default();
    let mut c1 = cell!(op.ntests);
    let mut c2 = TableCell::default();
    let mut c3 = TableCell::default();
    let mut c4 = TableCell::default();
    let mut c5 = TableCell::default();
    let mut c6 = TableCell::default();

    if op.ntests > 0 {
        // If autotests are available
        let mut pass = false;
        if op.generation == op.ntests {
            // If all generation tests succeed:
            add_color(&mut c2, op.generation, "green");
        } else {
            add_color(&mut c2, op.generation, "red");
        }
        if op.simulation == op.ntests {
            // If all simulation tests succeed:
            add_color(&mut c3, op.simulation, "green");
            add_color(&mut c4, format!("{:.0}%", op.percentage), "green");
            pass = true;
        } else {
            add_color(&mut c3, op.simulation, "red");
            add_color(&mut c4, format!("{:.0}%", op.percentage), "red");
        }
        if pass {
            // If both Generation and Simulation are OK,
            // Highlight very cell in green
            add_color(&mut c0, &op.id, "green");
        } else {
            // Otherwise, set to red
            add_color(&mut c0, &op.id, "red");
        }
    } else {
        c0.add_paragraph(&op.id);
        c2.add_paragraph(op.generation);
        c3.add_paragraph(op.simulation);
        c4.add_paragraph(format!("{:.0}%", op.percentage));
    }
    // Colorize 'Status' symbols:
    add_color(&mut c5, op.status, &op.status.color());

    // Compute 'Passed Last' column:
    if op.passed_last != "n/a" {
        let date = get_commit_date(&op.passed_last);
        c6.add_link(
            format!("{gitlab}/-/commit/{}", &op.passed_last),
            format!("#commit ({date})")
        );
    } else {
        c6.add_paragraph(&op.passed_last);
    }
    table.add_custom_body_row(
        TableRow::new()
            .with_cell(c0)
            .with_cell(c1)
            .with_cell(c2)
            .with_cell(c3)
            .with_cell(c4)
            .with_cell(c5)
            .with_cell(c6)
    );
}

fn read_parse_csv_file<P>(path: P) -> Vec<OperatorResults>
    where P: AsRef<Path> {
    // Read .csv file as String, from path:
    let mut vec = vec![];
    let csv = fs::read_to_string(path)
        .expect("Could not open/read summary.csv file");
    // Parse .csv file:
    let mut reader = csv::ReaderBuilder::new()
        .delimiter(b',')
        .from_reader(csv.as_bytes());

    for record in reader.deserialize() {
        vec.push(record.unwrap());
    }
    return vec;
}

fn write_csv_file<P: AsRef<Path>>(path: P, csv: &Vec<OperatorResults>) {
    let file = std::fs::OpenOptions::new()
        .create(true)
        .truncate(true)
        .read(true)
        .write(true)
        .open(path)
        .unwrap();
    let mut writer = csv::WriterBuilder::new()
        .has_headers(true)
        .from_writer(file);
    for op in csv {
        writer.serialize(&op).expect(
            "Could not write operator results entry to file, aborting"
        );
        writer.flush();
    }
}

fn write_html<P: AsRef<Path>>(path: P, csv: &Vec<OperatorResults>) {
    let mut table = build_html::Table::new()
        .with_attributes([("th style","text-align:center")]) // doesn't work
    ;
    // Add headers first (TODO: get them from serde instead):
    let headers = vec![
        "Operator", "Tests", "Generation",
        "Simulation", "%", "Status",
        "Passed Last"
    ];
    table.add_header_row(headers);
    // Add row for each operator:
    for op in csv {
        add_html_row(&op, &mut table);
    }
    // Get the code, and write to file:
    let html = table.to_html_string();
    fs::write("doc/web/autotests.html", &html)
        .expect("Could not write HTML output");
    println!("'doc/web/autotests.html' successfully generated!");
}

fn main() {
    // Read new .csv 'summary' file
    let mut summary = read_parse_csv_file("autotests/summary.csv");
    // Compare with previous results,
    // + update the 'status' and 'passed_last' members:
    let compare = read_parse_csv_file("doc/web/autotests.csv");
    for op in &mut summary {
        for cmp in &compare {
            if op.id == cmp.id {
                op.update(&cmp);
                break;
            }
        }
    }
    // Write/replace 'summary' with 'compare'.
    println!("Writing/updating 'doc/web/autotests.csv'");
    write_csv_file("doc/web/autotests.csv", &summary);
    // Convert and write to 'doc/web/autotests.html'
    println!("Converting to HTML: 'doc/web/autotests.html'");
    write_html("doc/web/autotests.html", &summary);
}
