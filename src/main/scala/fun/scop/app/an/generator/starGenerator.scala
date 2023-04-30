package fun.scop.app.an.generator

import fun.scop.app.an.util._
import java.io.PrintWriter

object starGenerator {

  def name(k: Int) = s"A${k}"

  def av(k: Int, value: Int) = AutomataValued(Automata(name(k)), value)

  def genAutomata(n: Int) = {
    for {
      k <- 1 to n
    } yield Automata(name(k))
  }

  def genDomain(n: Int) = {
    (for {
      k <- 1 to n
    } yield Automata(name(k)) -> Seq(0, 1)).toMap
  }

  def genTransition(n: Int) = {
    //  0 -> 1
    val ts01 = for {
      k <- 1 to n
    } yield Transition(
      av(k, 0),
      av(k, 1),
      (1 to n).filter(i => i != k).map(i => av(i, 0))
    )

    // 1 -> 0
    val ts10 = for {
      k <- 1 to n
    } yield Transition(
      av(k, 1),
      av(k, 0),
      (1 to n).filter(i => i != k).map(i => av(i, 0))
      // Seq.empty
    )
    val ts10simp = for {
      k <- 1 to n
    } yield Transition(
      av(k, 1),
      av(k, 0),
      Seq.empty
    )

    // ts01 ++ ts10
    ts01 ++ ts10simp
  }

  def generate(n: Int) =
    AutomataNetwork(genAutomata(n), genDomain(n), genTransition(n))

  def main(args: Array[String]) = {

    val an = generate(args(0).toInt)

    val pw = new PrintWriter(args(1))

    pw.write(an.toAnFormat)

    pw.close()

  }
}
