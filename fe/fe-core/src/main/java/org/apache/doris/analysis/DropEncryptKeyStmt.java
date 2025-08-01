// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

package org.apache.doris.analysis;

import org.apache.doris.catalog.EncryptKeySearchDesc;
import org.apache.doris.catalog.Env;
import org.apache.doris.common.ErrorCode;
import org.apache.doris.common.ErrorReport;
import org.apache.doris.common.UserException;
import org.apache.doris.mysql.privilege.PrivPredicate;
import org.apache.doris.qe.ConnectContext;

public class DropEncryptKeyStmt extends DdlStmt implements NotFallbackInParser {
    private final boolean ifExists;
    private final EncryptKeyName encryptKeyName;
    private EncryptKeySearchDesc encryptKeySearchDesc;

    public DropEncryptKeyStmt(boolean ifExists, EncryptKeyName encryptKeyName) {
        this.ifExists = ifExists;
        this.encryptKeyName = encryptKeyName;
    }

    public boolean isIfExists() {
        return ifExists;
    }

    public EncryptKeyName getEncryptKeyName() {
        return encryptKeyName;
    }

    public EncryptKeySearchDesc getEncryptKeysSearchDesc() {
        return encryptKeySearchDesc;
    }

    @Override
    public void analyze() throws UserException {
        super.analyze();

        // check operation privilege
        if (!Env.getCurrentEnv().getAccessManager().checkGlobalPriv(ConnectContext.get(), PrivPredicate.ADMIN)) {
            ErrorReport.reportAnalysisException(ErrorCode.ERR_SPECIFIC_ACCESS_DENIED_ERROR, "ADMIN");
        }

        // analyze encryptkey name
        encryptKeyName.analyze();
        encryptKeySearchDesc = new EncryptKeySearchDesc(encryptKeyName);
    }

    @Override
    public String toSql() {
        StringBuilder stringBuilder = new StringBuilder();
        stringBuilder.append("DROP ENCRYPTKEY ").append(encryptKeyName.getKeyName());
        return stringBuilder.toString();
    }

    @Override
    public StmtType stmtType() {
        return StmtType.DROP;
    }
}
