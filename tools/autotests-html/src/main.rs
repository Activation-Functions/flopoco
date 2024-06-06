use std::fs;
use build_html::{Html, HtmlContainer, TableCell, TableCellType, TableRow};
use serde::{Serialize, Deserialize, Deserializer};

#[derive(Default, Debug, Serialize, Deserialize)]
struct OperatorResults {
    #[serde(rename(deserialize = "Operator Name"))]
    id: String,
    // Note: the generated .csv file has ', ' as delimiter
    // which is impossible to set in the reader's options,
    // so it doesn't take the whitespace into account:
    #[serde(rename(deserialize = " Total Tests"))]
    #[serde(deserialize_with = "deserialize_trim")]
    ntests: i32,
    #[serde(rename(deserialize = " Generation OK"))]
    #[serde(deserialize_with = "deserialize_trim")]
    gen: i32,
    // Also Note: it's OK for 'Generation OK'
    // but 'Ok' for 'Simulation OK'... (TODO)
    #[serde(rename(deserialize = " Simulation Ok"))]
    #[serde(deserialize_with = "deserialize_trim")]
    sim: i32
}

fn deserialize_trim<'de, D>(deserializer: D) -> Result<i32, D::Error>
    where D: Deserializer<'de>
{
    Ok(String::deserialize(deserializer)
        .unwrap()
        .trim()
        .parse::<i32>()
        .unwrap()
    )
}

fn create_cell(txt: impl ToString, color: &str) -> TableCell {
    TableCell::new(TableCellType::Data)
        .with_paragraph_attr(
            txt.to_string().trim(),
            [("style", format!("color:{color}").as_str())]
        )
}

fn add_html_row(op: &OperatorResults, table: &mut build_html::Table) {
    // For each row:
    let mut c0 = TableCell::new(TableCellType::Data)
        .with_paragraph(&op.id);
    let mut c1 = TableCell::new(TableCellType::Data)
        .with_paragraph(op.ntests);
    let mut c2 = TableCell::new(TableCellType::Data)
        .with_paragraph(op.gen);
    let mut c3 = TableCell::new(TableCellType::Data)
        .with_paragraph(op.sim);
    if op.ntests > 0 {
        // If autotests are available
        let mut gen_ok = false;
        let mut sim_ok = false;
        if op.gen == op.ntests {
            // If all generation tests succeed:
            c2 = create_cell(op.gen, "green");
            gen_ok = true;
        } else {
            c2 = create_cell(op.gen, "red");
        }
        if op.sim == op.ntests {
            // If all simulation tests succeed:
            c3 = create_cell(op.sim, "green");
            sim_ok = true;
        } else {
            c3 = create_cell(op.sim, "red");
        }
        if gen_ok && sim_ok {
            // If both Generation and Simulation are OK,
            // Highlight very cell in green
            c0 = create_cell(&op.id, "green");
        } else {
            // Otherwise, set to red
            c0 = create_cell(&op.id, "red");
        }
    }
    table.add_custom_body_row(
        TableRow::new()
            .with_cell(c0)
            .with_cell(c1)
            .with_cell(c2)
            .with_cell(c3)
    );
}

fn main() {
    let mut global = OperatorResults::default();
    global.id = String::from("Total");
    let mut html_table = build_html::Table::new();
    // Read .csv file as String, from current directory:
    let csv = fs::read_to_string("summary.csv")
        .expect("Could not open/read summary.csv file")
    ;
    // Parse .csv file:
    let mut reader = csv::ReaderBuilder::new()
        .delimiter(b',')
        .from_reader(csv.as_bytes())
    ;
    match reader.headers() {
    // Parse and add headers:
        Ok(record) => {
            let header: Vec<&str> = record.iter().collect();
            html_table.add_header_row(header);
        }
        Err(..) => ()
    }
    for record in reader.deserialize() {
        let op: OperatorResults = record.unwrap();
        add_html_row(&op, &mut html_table);
        global.ntests += op.ntests;
        global.gen += op.gen;
        global.sim += op.sim;
    }
    add_html_row(&global, &mut html_table);
    let html = html_table.to_html_string();
    fs::write("autotests.html", &html).expect("Could not write HTML output");
    println!("'autotests.html' successfully generated!");
}
