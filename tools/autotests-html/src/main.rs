use std::fs;
use std::path::Path;
use std::fmt::Debug;
use build_html::{Html, HtmlContainer, TableCell, TableCellType, TableRow};
use serde::{Serialize, Deserialize};
use std::process::Command;
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

const STATUS_UP_OPCODE: &str = "&#8593";
const STATUS_DN_OPCODE: &str = "&#8595";

/// This would be the HTML representation of the Operator Status:
impl ToString for OperatorStatus {
    fn to_string(&self) -> String {
        match self {
               Self::Up => String::from(STATUS_UP_OPCODE),
             Self::Down => String::from(STATUS_DN_OPCODE),
             Self::Pass => String::from("PASS!"),
            Self::Break => String::from("BREAK!"),
           Self::Stable => String::from("="),
        Self::Undefined => String::from("n/a")
        }
    }
}

/// HTML status color.
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
    // Update: According to RFC 4180, spaces outside quotes
    // in a field are not allowed.
    // See also: https://github.com/BurntSushi/rust-csv/issues/210
    // multi-character delimiters are now removed.
    #[serde(rename = "Tests")]
    ntests: usize,
    // ---------------------------------------------------
    #[serde(rename = "Generation")]
    generation: usize,
    // ---------------------------------------------------
    #[serde(rename = "Simulation")]
    simulation: usize,
    // ---------------------------------------------------
    #[serde(rename = "%")]
    percentage: f32,
    //----------------------------------------------------
    #[serde(rename = "Status")]
    status: OperatorStatus,
    // ---------------------------------------------------
    #[serde(rename = "Changed Last")]
    changed_last: String, // commit hash
    // ---------------------------------------------------
    #[serde(rename = "Status Last")]
    status_last: OperatorStatus, // commit hash
}

const GITLAB: &str = "https://gitlab.com/flopoco/flopoco";

fn get_commit_date(commit: &String) -> String {
    let out = Command::new("git")
        .arg("show").arg("-s")
        .arg("--format=%ci")
        .arg(&commit)
        .output()
        .expect("Could not get commit date")
        .stdout;
    return String::from_utf8(out)
        .expect("Could not parse commit date");
}

fn get_current_branch() -> String {
    let out = Command::new("git")
        .arg("symbolic-ref")
        .arg("--short").arg("HEAD")
        .output()
        .expect("Could not get current git branch")
        .stdout;
    return String::from_utf8(out)
        .expect("Could not parse current git branch");
}

fn get_commit_hash() -> String {
    let out = Command::new("git")
        .arg("rev-parse").arg("HEAD")
        .output()
        .expect("Failed to get current git commit")
        .stdout;
    let mut s = String::from_utf8(out)
        .expect("Could not parse git output");
    s.pop();
    return s;
}

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
        } else if self.percentage > cmp.percentage {
            self.status = OperatorStatus::Up;
        } else if self.percentage < cmp.percentage {
            self.status = OperatorStatus::Down;
        } else {
            self.status = OperatorStatus::Stable;
        }
        if self != &cmp {
            println!("Change for Operator: {:?}", cmp.status);
            self.changed_last = get_commit_hash();
            self.status_last = cmp.status;
        } else {
            self.changed_last = cmp.changed_last.clone();
            self.status_last = cmp.status_last;
        }
        // println!("Updating Operator {}, status: {:?}",
        //     self.id, self.status
        // );
    }
}

impl std::ops::AddAssign<&OperatorResults> for OperatorResults {
    fn add_assign(&mut self, other: &OperatorResults) {
        self.ntests += other.ntests;
        self.generation += other.generation;
        self.simulation += other.simulation;
    }
}

impl PartialEq<&OperatorResults> for OperatorResults {
    fn eq(&self, other: &&OperatorResults) -> bool {
        return self.ntests == other.ntests
            && self.generation == other.generation
            && self.simulation == other.simulation;
    }
}

fn cell_color(cell: &mut TableCell, txt: impl ToString, color: &str) {
    cell.add_paragraph_attr(
        txt.to_string().trim(), [
            ("style", format!("color:{color}").as_str())
        ]
    );
}

macro_rules! cell {
    ($contents:expr) => {
        TableCell::new(TableCellType::Data)
            .with_paragraph($contents)
    };
}

#[allow(unused_mut)]
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
    let mut c7 = TableCell::default();

    // If autotests are available for operator
    if op.ntests > 0 {
        let mut pass = false;
        if op.generation == op.ntests {
            // If all generation tests succeed:
            cell_color(&mut c2, op.generation, "green");
        } else {
            cell_color(&mut c2, op.generation, "red");
        }
        if op.simulation == op.ntests {
            // If all simulation tests succeed:
            cell_color(&mut c3, op.simulation, "green");
            cell_color(&mut c4, format!("{:.0}%", op.percentage), "green");
            pass = true;
        } else {
            cell_color(&mut c3, op.simulation, "red");
            cell_color(&mut c4, format!("{:.0}%", op.percentage), "red");
        }
        if pass {
            // If both Generation and Simulation are OK,
            // Highlight every cell in green
            cell_color(&mut c0, &op.id, "green");
        } else {
            // Otherwise, set to red
            cell_color(&mut c0, &op.id, "red");
        }
    } else {
        c0.add_paragraph(&op.id);
        c2.add_paragraph(op.generation);
        c3.add_paragraph(op.simulation);
        c4.add_paragraph(format!("{:.0}%", op.percentage));
    }
    // Colorize 'Status' symbols:
    cell_color(&mut c5, op.status, &op.status.color());
    cell_color(&mut c7, op.status_last, &op.status_last.color());
    // Compute 'Passed Last' column:
    if op.changed_last != "n/a" {
        let date = get_commit_date(&op.changed_last);
        c6.add_link(
            format!("{GITLAB}/-/commit/{}", &op.changed_last),
            format!("#commit ({date})")
        );
    } else {
        c6.add_paragraph(&op.changed_last);
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
            .with_cell(c7)
    );
}

fn read_parse_csv_file<P>(path: P) -> Vec<OperatorResults>
    where P: AsRef<Path>
{
    let mut vec = vec![];
    // Read .csv file as String, from path:
    let csv = fs::read_to_string(path.as_ref())
        .expect(&format!(
            "Could not open/read {:?}",
            path.as_ref().display()
        ).to_string()
    );
    // Parse .csv file:
    let mut reader = csv::ReaderBuilder::new()
        .delimiter(b',')
        .from_reader(csv.as_bytes());
    // Deserialize in new Vec
    for record in reader.deserialize() {
        vec.push(record.unwrap());
    }
    return vec;
}

fn write_csv_file<P>(path: P, csv: &Vec<OperatorResults>)
    where P: AsRef<Path>
{
    let file = std::fs::OpenOptions::new()
        .create(true)
        .truncate(true)
        .read(true)
        .write(true)
        .open(path.as_ref())
        .unwrap();
    let mut writer = csv::WriterBuilder::new()
        .has_headers(true)
        .from_writer(file);
    for op in csv {
        writer.serialize(&op).expect(
            "Could not write operator results entry to file, aborting"
        );
        let _ = writer.flush();
    }
}

fn write_html<P>(path: P, csv: &Vec<OperatorResults>)
    where P: AsRef<Path>
{
    let date = chrono::offset::Local::now();
    let branch = get_current_branch();
    let mut page = build_html::HtmlPage::new()
        .with_title("FloPoCo autotest results")
        .with_header(3, format!("generated: {date}"))
        .with_header(3, format!("branch: <em>{branch}</em>"))
    ;
    let mut table = build_html::Table::new()
        .with_attributes([("th style","text-align:center")]);
    // Add headers first (TODO: get them from serde instead):
    let headers = vec![
        "Operator", "Tests", "Generation",
        "Simulation", "%", "Status",
        "Changed Last", "Status Last"
    ];
    table.add_header_row(headers);
    // Add row for each operator:
    for op in csv {
        add_html_row(&op, &mut table);
    }
    // Get the code, and write to file:
    page.add_table(table);
    let html = page.to_html_string();
    fs::write(path.as_ref(), &html)
        .expect("Could not write HTML output");
    println!("'{:?}' successfully generated!",
            path.as_ref().display());
}

fn main() {
    // Read new .csv 'summary' file
    let mut summary = read_parse_csv_file("autotests/summary.csv");
    // Compare with previous results,
    // + update the 'status' and 'changed_last' members:
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
