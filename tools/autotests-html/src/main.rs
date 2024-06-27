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
    // Also Note: it's OK for 'Generation OK'
    // but 'Ok' for 'Simulation OK'... (TODO)
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
    passed_last: isize, // commit hash
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

fn create_cell(txt: impl ToString, color: &str) -> TableCell {
    TableCell::new(TableCellType::Data)
        .with_paragraph_attr(
            txt.to_string().trim(), [(
                "style", format!("color:{color}").as_str()
            )]
        )
}

macro_rules! cell {
    ($contents:expr) => {
        TableCell::new(TableCellType::Data)
            .with_paragraph($contents)
    };
}

fn add_html_row(op: &OperatorResults, table: &mut build_html::Table) {
    // For each row:
    // Note: TableCell doesn't implement copy/clone
    // so we have to instantiate each cell individually...
    let mut c0 = cell!(&op.id);
    let mut c1 = cell!(op.ntests);
    let mut c2 = cell!(op.generation);
    let mut c3 = cell!(op.simulation);
    let mut c4 = cell!(op.percentage);
    let mut c5 = cell!(op.status);
    let mut c6 = cell!(op.passed_last);

    if op.ntests > 0 {
        // If autotests are available
        let mut gen_ok = false;
        let mut sim_ok = false;
        if op.generation == op.ntests {
            // If all generation tests succeed:
            c2 = create_cell(op.generation, "green");
            gen_ok = true;
        } else {
            c2 = create_cell(op.generation, "red");
        }
        if op.simulation == op.ntests {
            // If all simulation tests succeed:
            c3 = create_cell(op.simulation, "green");
            c4 = create_cell(format!("{:.0}%", op.percentage), "green");
            sim_ok = true;
        } else {
            c3 = create_cell(op.simulation, "red");
            c4 = create_cell(format!("{:.0}%", op.percentage), "red");
        }
        if gen_ok && sim_ok {
            // If both Generation and Simulation are OK,
            // Highlight very cell in green
            c0 = create_cell(&op.id, "green");
        } else {
            // Otherwise, set to red
            c0 = create_cell(&op.id, "red");
        }
    } else {
        c4 = create_cell(format!("{:.0}%", op.percentage), "black");
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
    let mut table = build_html::Table::new();
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
    // Read new .csv 'summary' file, add total row:
    let mut summary = read_parse_csv_file("autotests/summary.csv");
    let compare = read_parse_csv_file("doc/web/autotests.csv");
    // Compare with previous results,
    // + update the 'status' and 'passed_last' members:
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
    // match csv_summary.headers() {
    // // Parse and add headers:
    //     Ok(record) => {
    //         let mut headers: Vec<&str> = record.iter().collect();
    //         html_table.add_header_row(headers);
    //     }
    //     Err(..) => ()
    // }
}
