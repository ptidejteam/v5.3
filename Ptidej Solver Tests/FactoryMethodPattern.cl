[customProblemForFactoryMethodModel() : PtidejProblem
    ->  verbose() := 0,
        let pb := makePtidejProblem("FactoryMethod Model Problem", length(listOfEntities), 90),
            concreteCreatorVar := makePtidejVar(pb, "ConcreteCreator", 1, length(listOfEntities)),
            concreteProductVar := makePtidejVar(pb, "ConcreteProduct", 1, length(listOfEntities)),
            creatorVar := makePtidejVar(pb, "Creator", 1, length(listOfEntities)),
            productVar := makePtidejVar(pb, "Product", 1, length(listOfEntities)) in (

            setVarsToShow(pb.globalSearchSolver, pb.vars),

            post(pb,
                 makeAssociationConstraint(
                    "ConcreteCreator ----> Product",
                    "throw new RuntimeException(\"ConcreteCreator should ----> Product\");",
                    concreteCreatorVar,
                    productVar),
                 100),
            post(pb,
                 makeCreationConstraint(
                    "ConcreteCreator -*--> ConcreteProduct",
                    "throw new RuntimeException(\"ConcreteCreator should -*--> ConcreteProduct\");",
                    concreteCreatorVar,
                    concreteProductVar),
                 100),
            post(pb,
                 makeUseConstraint(
                    "Creator -k--> Product",
                    "throw new RuntimeException(\"Creator should -k--> Product\");",
                    creatorVar,
                    productVar),
                 100),
            post(pb,
                 makeAssociationConstraint(
                    "Creator ----> Product",
                    "throw new RuntimeException(\"Creator should ----> Product\");",
                    creatorVar,
                    productVar),
                 100),
            post(pb,
                 makeStrictInheritanceConstraint(
                    "ConcreteCreator -|>- Creator",
                    "ConcreteCreator, Creator | javaXL.XClass c1, javaXL.XClass c2 | c1.setSuperclass(c2.getName());",
                    concreteCreatorVar,
                    creatorVar),
                 50),
            post(pb,
                 makeStrictInheritanceConstraint(
                    "ConcreteProduct -|>- Product",
                    "ConcreteProduct, Product | javaXL.XClass c1, javaXL.XClass c2 | c1.setSuperclass(c2.getName());",
                    concreteProductVar,
                    productVar),
                 50),
            post(pb,
                 makeIgnoranceConstraint(
                    "ConcreteCreator -/--> [C@71a29452",
                    "throw new RuntimeException(\"ConcreteCreator -/--> [C@71a29452\");",
                    concreteCreatorVar,
                    creatorVar),
                 75),
            post(pb,
                 makeIgnoranceConstraint(
                    "ConcreteProduct -/--> [C@1409d7f5",
                    "throw new RuntimeException(\"ConcreteProduct -/--> [C@1409d7f5\");",
                    concreteProductVar,
                    concreteCreatorVar),
                 75),
            post(pb,
                 makeIgnoranceConstraint(
                    "ConcreteProduct -/--> [C@71a29452",
                    "throw new RuntimeException(\"ConcreteProduct -/--> [C@71a29452\");",
                    concreteProductVar,
                    creatorVar),
                 75),
            post(pb,
                 makeIgnoranceConstraint(
                    "ConcreteProduct -/--> [C@68b7cdc6",
                    "throw new RuntimeException(\"ConcreteProduct -/--> [C@68b7cdc6\");",
                    concreteProductVar,
                    productVar),
                 75),
            post(pb,
                 makeIgnoranceConstraint(
                    "Creator -/--> [C@1409d7f5",
                    "throw new RuntimeException(\"Creator -/--> [C@1409d7f5\");",
                    creatorVar,
                    concreteCreatorVar),
                 75),
            post(pb,
                 makeIgnoranceConstraint(
                    "Creator -/--> [C@1241201a",
                    "throw new RuntimeException(\"Creator -/--> [C@1241201a\");",
                    creatorVar,
                    concreteProductVar),
                 75),
            post(pb,
                 makeIgnoranceConstraint(
                    "Product -/--> [C@1409d7f5",
                    "throw new RuntimeException(\"Product -/--> [C@1409d7f5\");",
                    productVar,
                    concreteCreatorVar),
                 75),
            post(pb,
                 makeIgnoranceConstraint(
                    "Product -/--> [C@1241201a",
                    "throw new RuntimeException(\"Product -/--> [C@1241201a\");",
                    productVar,
                    concreteProductVar),
                 75),
            post(pb,
                 makeIgnoranceConstraint(
                    "Product -/--> [C@71a29452",
                    "throw new RuntimeException(\"Product -/--> [C@71a29452\");",
                    productVar,
                    creatorVar),
                 75),
            post(pb,
                 makeNotEqualConstraint(
                    "ConcreteCreator <> ConcreteProduct",
                    "throw new RuntimeException(\"ConcreteCreator <> ConcreteProduct\");",
                    concreteCreatorVar,
                    concreteProductVar),
                 100),
            post(pb,
                 makeNotEqualConstraint(
                    "ConcreteCreator <> Creator",
                    "throw new RuntimeException(\"ConcreteCreator <> Creator\");",
                    concreteCreatorVar,
                    creatorVar),
                 100),
            post(pb,
                 makeNotEqualConstraint(
                    "ConcreteCreator <> Product",
                    "throw new RuntimeException(\"ConcreteCreator <> Product\");",
                    concreteCreatorVar,
                    productVar),
                 100),
            post(pb,
                 makeNotEqualConstraint(
                    "ConcreteProduct <> Creator",
                    "throw new RuntimeException(\"ConcreteProduct <> Creator\");",
                    concreteProductVar,
                    creatorVar),
                 100),
            post(pb,
                 makeNotEqualConstraint(
                    "ConcreteProduct <> Product",
                    "throw new RuntimeException(\"ConcreteProduct <> Product\");",
                    concreteProductVar,
                    productVar),
                 100),
            post(pb,
                 makeNotEqualConstraint(
                    "Creator <> Product",
                    "throw new RuntimeException(\"Creator <> Product\");",
                    creatorVar,
                    productVar),
                 100),
            pb
        )
]