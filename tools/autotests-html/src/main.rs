
// Usage: autotests-html:
// - run flopoco autotest operator=all testlevel=0
// - get the output summary result in /tmp
// - parse csv, convert to html
// - for every row, parse total/generation and total/simulation
// - if they're the same, highlight the cell green
// - otherwise, highlight the cell red


use std::fs;
use build_html::{Html, HtmlContainer, TableCell, TableCellType, TableRow};

fn main() {
    let mut html_table = build_html::Table::new();
    // Read .csv file
    let fcsv = fs::read_to_string("summary.csv")
        .expect("Could not open/read summary.csv file")
    ;
    let mut rdr = csv::ReaderBuilder::new()
        .delimiter(b',')
        .from_reader(fcsv.as_bytes())
    ;
    match rdr.headers() {
    // Parse and add headers:
        Ok(record) => {
            let hdr: Vec<&str> = record.iter().collect();
            html_table.add_header_row(hdr);
        }
        Err(..) => ()
    }
    // Collect records:
    rdr.records().into_iter().for_each(|r| match r {
        Ok(record) => {
            // Get generation cell and
            // Get 'Total'
            // println!("record = {:?}", record);
            let n = record[0].parse::<String>().unwrap();
            let t = record[1].trim().parse::<i32>().unwrap();
            let g = record[2].trim().parse::<i32>().unwrap();
            let s = record[3].trim().parse::<i32>().unwrap();
            let cn = TableCell::new(TableCellType::Data)
                .with_paragraph(n)
            ;
            let ct = TableCell::new(TableCellType::Data)
                .with_paragraph(t)
            ;
            let mut cg = TableCell::new(TableCellType::Data);
            let mut cs = TableCell::new(TableCellType::Data);
            if t > 0 {
                if g == t {
                    // get the cell, color it green
                    cg.add_paragraph_attr(record[2].trim(), [("style", "color:green")]);
                } else {
                    // get the cell, color it red...
                    cg.add_paragraph_attr(record[2].trim(), [("style", "color:red")]);
                }
                // Same for 'Simulation'
                if s == t {
                    cs.add_paragraph_attr(record[3].trim(), [("style", "color:green")]);
                } else {
                    cs.add_paragraph_attr(record[3].trim(), [("style", "color:red")]);
                }
            } else {
                cg.add_paragraph(record[2].trim());
                cs.add_paragraph(record[3].trim());
            }
            // Add body row
            html_table.add_custom_body_row(
                TableRow::new()
                    .with_cell(cn)
                    .with_cell(ct)
                    .with_cell(cg)
                    .with_cell(cs)
            );
        }
        Err(..) => (),
    });
    let html = html_table.to_html_string();
    fs::write("autotests.html", &html).expect("Could not write HTML output");
    println!("{}", &html);
}
